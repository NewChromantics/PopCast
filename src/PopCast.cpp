#include "PopCast.h"
#include "TCaster.h"
#include "TBlitter.h"
#include "TAirplayCaster.h"
#include "PopUnity.h"
#include "TFileCaster.h"
#include <SoyJson.h>
#include <SoyExportManager.h>

#if defined(TARGET_OSX)
#define ENABLE_GOOGLECAST
#define ENABLE_AIRPLAY
#endif

#if defined(ENABLE_GOOGLECAST)
#include "TGoogleCaster.h"
#endif

#if defined(ENABLE_AIRPLAY)
#include "TAirplayCaster.h"
#endif

#if defined(TARGET_OSX)||defined(TARGET_IOS)
#include "AvfCompressor.h"
#endif


namespace PopCast
{
	std::mutex gInstancesLock;
	std::vector<std::shared_ptr<PopCast::TInstance> >	gInstances;
	
#if defined(ENABLE_GOOGLECAST)
	std::shared_ptr<GoogleCast::TContext>	GoogleCastContext;
	GoogleCast::TContext&					GetGoogleCastContext();
#endif

#if defined(ENABLE_AIRPLAY)
	std::shared_ptr<Airplay::TContext>		AirplayContext;
	Airplay::TContext&						GetAirplayContext();
#endif

	void		GetMeta(TJsonWriter& Json);

	std::shared_ptr<TExportManager<std::string,char>>	gExportStringManager;
	TExportManager<std::string,char>&					GetExportStringManager();
};


TExportManager<std::string, char>& PopCast::GetExportStringManager()
{
	if ( !gExportStringManager )
		gExportStringManager.reset( new TExportManager<std::string,char>(100) );

	return *gExportStringManager;
}



bool HasBit(Unity::uint ParamBits,TPluginParams::PopCastFlags Param)
{
	auto Masked = (ParamBits & Param);
	return Masked != 0;
}

TCasterParams MakeCasterParams(Unity::uint ParamBits,const char* Filename,float RateMegaBytesPerSec)
{
	//	decoder params
	TCasterParams Params;
	
	Params.mName = Filename;

	Params.mShowFinishedFile = HasBit( ParamBits, TPluginParams::ShowFinishedFile );
	Params.mSkipFrames = HasBit( ParamBits, TPluginParams::SkipFrames );
	Params.mGifParams.mAllowIntraFrames = HasBit( ParamBits, TPluginParams::Gif_AllowIntraFrames );
	Params.mGifParams.mDebugPalette = HasBit( ParamBits, TPluginParams::Gif_DebugPalette );
	Params.mGifParams.mDebugIndexes = HasBit( ParamBits, TPluginParams::Gif_DebugIndexes );
	Params.mGifParams.mDebugTransparency = HasBit(ParamBits, TPluginParams::Gif_DebugTransparency);
	Params.mGifParams.mCpuOnly = HasBit(ParamBits, TPluginParams::Gif_CpuOnly);
	Params.mGifParams.mLzwCompression = HasBit(ParamBits, TPluginParams::Gif_LzwCompression);

	//	gr: this is here to make gif stuff simpler
	//	force watermark palette
	if ( Soy::TBlitter::HasWatermark() )
	{
		auto LimeColour = Soy::TRgb8( 218.0, 245.0, 25.0 );
		auto StickColour = Soy::TRgb8( 191.0, 147.0, 79.0 );
		auto StickShadowColour = Soy::TRgb8( 103.0, 80.0, 43.0 );
		auto HighlightColour = Soy::TRgb8( 255.0, 255.0, 255.0 );

		Params.mGifParams.mForcedPaletteColours.PushBack( LimeColour );
		Params.mGifParams.mForcedPaletteColours.PushBack( StickColour );
		Params.mGifParams.mForcedPaletteColours.PushBack( StickShadowColour );
		Params.mGifParams.mForcedPaletteColours.PushBack( HighlightColour );
	}



	Params.mMpegParams.mCodec = SoyMediaFormat::H264_ES;
	
	//	60fps is just black on mediafoundation...
	Params.mMpegParams.SetBitRateMegaBytesPerSecond( RateMegaBytesPerSec );
	static int fps = 30;
	Params.mMpegParams.mFrameRate = fps;

	return Params;
}


__export Unity::uint	PopCast_Alloc(const char* Filename,Unity::uint ParamBits,Unity::Float RateMegaBytesPerSec)
{
	ofScopeTimerWarning Timer(__func__, Unity::mMinTimerMs );
	if ( Filename == nullptr )
	{
		std::Debug << "Tried to allocate PopCast with null filename" << std::endl;
		return 0;
	}
	
	auto Params = MakeCasterParams( ParamBits, Filename, RateMegaBytesPerSec );

	try
	{
		auto Instance = PopCast::Alloc( Params );
		if ( !Instance )
			return 0;
		return Instance->GetRef();
	}
	catch ( std::exception& e )
	{
		std::Debug << "Failed to allocate movie: " << e.what() << std::endl;
		return 0;
	}
	/*
	catch (...)
	{
		std::Debug << "Failed to allocate movie: Unknown exception in " << __func__ << std::endl;
		return 0;
	}
	 */
	
	return 0;
}

__export bool	PopCast_Free(Unity::uint Instance)
{
	ofScopeTimerWarning Timer(__func__, Unity::mMinTimerMs );
	return PopCast::Free( Instance );
}


__export Unity::sint	PopCast_GetPendingFrameCount(Unity::uint Instance)
{
	auto pInstance = PopCast::GetInstance(Instance);
	if ( !pInstance )
		return -1;

	try
	{
		return pInstance->GetPendingPacketCount();
	}
	catch ( std::exception& e )
	{
		std::Debug << __func__ << " failed: " << e.what() << std::endl;
		return -2;
	}
}


__export Unity::uint	PopCast_GetBackgroundGpuJobCount()
{
	Unity::uint JobCount = 0;

	try
	{
		auto OpenglContext = Unity::GetOpenglContextPtr();
		if ( OpenglContext )
			JobCount += OpenglContext->GetJobCount();

#if defined(ENABLE_DIRECTX)
		auto DirectxContext = Unity::GetDirectxContextPtr();
		if ( DirectxContext )
			JobCount += DirectxContext->GetJobCount();
#endif
	}
	catch (std::exception& e)
	{
		std::Debug << __func__ << " exception: " << e.what() << std::endl;
	}

	return JobCount;
}


__export void	PopCast_EnumDevices()
{
	Array<TCastDeviceMeta> Metas;

	//	get chromecast devices
#if defined(ENABLE_GOOGLECAST)
	try
	{
		auto& GoogleCastContext = PopCast::GetGoogleCastContext();
		GoogleCastContext.EnumDevices( GetArrayBridge( Metas ) );
	}
	catch (std::exception& e)
	{
		std::Debug << e.what() << std::endl;
	}
#endif

#if defined(ENABLE_AIRPLAY)
	try
	{
		auto& AirplayContext = PopCast::GetAirplayContext();
		AirplayContext.EnumDevices( GetArrayBridge( Metas ) );
	}
	catch (std::exception& e)
	{
		std::Debug << e.what() << std::endl;
	}
#endif
	
	for ( int i=0;	i<Metas.GetSize();	i++ )
	{
		std::Debug << Metas[i] << std::endl;
	}
}

__export bool	PopCast_UpdateRenderTexture(Unity::uint Instance,Unity::NativeTexturePtr TextureId,Unity::sint Width,Unity::sint Height,Unity::RenderTexturePixelFormat::Type PixelFormat,Unity::sint StreamIndex)
{
	auto pInstance = PopCast::GetInstance( Instance );
	if ( !pInstance )
		return false;

	try
	{
		SoyPixelsMeta Meta( Width, Height, Unity::GetPixelFormat( PixelFormat ) );
		auto OpenglContext = Unity::GetOpenglContextPtr();
#if defined(ENABLE_DIRECTX)
		auto DirectxContext = Unity::GetDirectxContextPtr();
#endif
		
		if ( OpenglContext )
		{
			//	assuming type atm... maybe we can extract it via opengl?
			GLenum Type = GL_TEXTURE_2D;
			Opengl::TTexture Texture( TextureId, Meta, Type );
			pInstance->WriteFrame( Texture, size_cast<size_t>(StreamIndex) );
			return true;
		}

#if defined(ENABLE_DIRECTX)
		if ( DirectxContext )
		{
			Directx::TTexture Texture( static_cast<ID3D11Texture2D*>(TextureId) );
			pInstance->WriteFrame( Texture, size_cast<size_t>(StreamIndex) );
			return true;
		}
#endif
		
		throw Soy::AssertException("Missing graphics device");
	}
	catch(std::exception& e)
	{
		std::Debug << __func__ << " failed: " << e.what() << std::endl;
		return false;
	}
}
__export bool	PopCast_UpdateTexture2D(Unity::uint Instance,Unity::NativeTexturePtr TextureId,Unity::sint Width,Unity::sint Height,Unity::Texture2DPixelFormat::Type PixelFormat,Unity::sint StreamIndex)
{
	auto pInstance = PopCast::GetInstance( Instance );
	if ( !pInstance )
		return false;
	
	try
	{
		SoyPixelsMeta Meta( Width, Height, Unity::GetPixelFormat( PixelFormat ) );
		auto OpenglContext = Unity::GetOpenglContextPtr();
#if defined(ENABLE_DIRECTX)
		auto DirectxContext = Unity::GetDirectxContextPtr();
#endif

		if ( OpenglContext )
		{
			//	assuming type atm... maybe we can extract it via opengl?
			GLenum Type = GL_TEXTURE_2D;
			Opengl::TTexture Texture( TextureId, Meta, Type );
			pInstance->WriteFrame( Texture, size_cast<size_t>(StreamIndex) );
			return true;
		}

#if defined(ENABLE_DIRECTX)
		if ( DirectxContext )
		{
			Directx::TTexture Texture( static_cast<ID3D11Texture2D*>(TextureId) );
			pInstance->WriteFrame( Texture, size_cast<size_t>(StreamIndex) );
			return true;
		}
#endif
		throw Soy::AssertException("Missing graphics device");
	}
	catch(std::exception& e)
	{
		std::Debug << __func__ << " failed: " << e.what() << std::endl;
		return false;
	}
}

__export bool PopCast_UpdateTextureDebug(Unity::uint Instance,Unity::sint StreamIndex)
{
	auto pInstance = PopCast::GetInstance( Instance );
	if ( !pInstance )
		return false;
	
	try
	{
		//	make up a debug image
		SoyPixelsMeta Meta( 640, 480, SoyPixelsFormat::RGB );
		auto pPixels = std::make_shared<SoyPixels>();
		auto& Pixels = *pPixels;
		Pixels.Init( Meta );

		static Soy::TRgba8 Colours[] =
		{
			Soy::TRgba8(255, 0, 0, 255),
			Soy::TRgba8(255, 255, 0, 255),
			Soy::TRgba8(0, 255, 0, 255),
			Soy::TRgba8(0, 255, 255, 255),
			Soy::TRgba8(0, 0, 255, 255),
			Soy::TRgba8(255, 0, 255, 255),
		};
		static int ColourIndex = 0;
		auto Colour = Colours[(ColourIndex++) % sizeofarray(Colours)];
		Pixels.SetPixels( GetArrayBridge(Colour.GetArray()) );

		pInstance->WriteFrame( pPixels, size_cast<size_t>(StreamIndex) );
		return true;
	}
	catch(std::exception& e)
	{
		std::Debug << __func__ << " failed: " << e.what() << std::endl;
		return false;
	}
}



__export const char*	PopCast_GetMetaJson(Unity::uint Instance)
{
	try
	{
		TJsonWriter Json;

		PopCast::GetMeta( Json );

		auto pInstance = PopCast::GetInstance( Instance );
		if ( pInstance )
		{
			pInstance->GetMeta(Json);
		}
		
		auto& StringManager = PopCast::GetExportStringManager();
		return StringManager.Lock( Json.GetString() );
	}
	catch ( std::exception& e )
	{
		std::Debug << __func__ << " exception " << e.what() << std::endl;
		return nullptr;
	}
}


__export void	PopCast_ReleaseString(const char* String)
{
	try
	{
		auto& StringManager = PopCast::GetExportStringManager();
		StringManager.Unlock( String );
	}
	catch ( std::exception& e )
	{
		std::Debug << __func__ << " exception " << e.what() << std::endl;
	}
}

__export const char* PopCast_PopDebugString()
{
	try
	{
		return Unity::PopDebugString();
	}
	catch(std::exception& e)
	{
		//	gr: this could get a bit recursive
		std::Debug << __func__ << " exception " << e.what() << std::endl;
		return nullptr;
	}
}

__export void PopCast_ReleaseDebugString(const char* String)
{
	try
	{
		Unity::ReleaseDebugString( String );
	}
	catch(std::exception& e)
	{
		//	gr: this could get a bit recursive
		std::Debug << __func__ << " exception " << e.what() << std::endl;
	}
}

__export void PopCast_ReleaseAllExports()
{
	try
	{
		Unity::ReleaseDebugStringAll();
	}
	catch(std::exception& e)
	{
		//	gr: this could get a bit recursive
		std::Debug << __func__ << " exception " << e.what() << std::endl;
	}
}



#if defined(ENABLE_GOOGLECAST)
GoogleCast::TContext& PopCast::GetGoogleCastContext()
{
	if ( !PopCast::GoogleCastContext )
	{
		PopCast::GoogleCastContext.reset( new GoogleCast::TContext() );
	}
	
	return *PopCast::GoogleCastContext;
}
#endif


#if defined(ENABLE_AIRPLAY)
Airplay::TContext& PopCast::GetAirplayContext()
{
	if ( !PopCast::AirplayContext )
	{
		PopCast::AirplayContext.reset( new Airplay::TContext() );
	}
	
	return *PopCast::AirplayContext;
}
#endif


std::shared_ptr<PopCast::TInstance> PopCast::Alloc(TCasterParams Params,std::shared_ptr<Opengl::TContext> OpenglContext,std::shared_ptr<Directx::TContext> DirectxContext)
{
	gInstancesLock.lock();
	static TInstanceRef gInstanceRefCounter(1000);
	auto InstanceRef = gInstanceRefCounter++;
	gInstancesLock.unlock();

	if ( !OpenglContext )
		OpenglContext = Unity::GetOpenglContextPtr();

#if defined(ENABLE_DIRECTX)
	if ( !DirectxContext )
		DirectxContext = Unity::GetDirectxContextPtr();
#endif
	std::shared_ptr<TInstance> pInstance( new TInstance(InstanceRef,Params,OpenglContext,DirectxContext) );
	
	gInstancesLock.lock();
	gInstances.push_back( pInstance );
	gInstancesLock.unlock();

	return pInstance;
}

std::shared_ptr<PopCast::TInstance> PopCast::GetInstance(TInstanceRef Instance)
{
	for ( int i=0;	i<gInstances.size();	i++ )
	{
		auto& pInstance = gInstances[i];
		if ( pInstance->GetRef() == Instance )
			return pInstance;
	}
	return std::shared_ptr<PopCast::TInstance>();
}

bool PopCast::Free(TInstanceRef Instance)
{
	gInstancesLock.lock();
	for ( int i=0;	i<gInstances.size();	i++ )
	{
		auto& pInstance = gInstances[i];
		if ( pInstance->GetRef() != Instance )
			continue;
		
		if ( pInstance )
		{
			pInstance.reset();
		}
		gInstances.erase( gInstances.begin()+ i );
		gInstancesLock.unlock();
		return true;
	}
	gInstancesLock.unlock();
	return false;
}



void PopCast::GetMeta(TJsonWriter& Json)
{
	auto BackgroundGpuJobCount = PopCast_GetBackgroundGpuJobCount();
	auto InstanceCount = gInstances.size();

	Json.Push("BackgroundGpuJobCount", BackgroundGpuJobCount);
	Json.Push("InstanceCount", InstanceCount);
}



PopCast::TInstance::TInstance(const TInstanceRef& Ref,TCasterParams& _Params,std::shared_ptr<Opengl::TContext> OpenglContext,std::shared_ptr<Directx::TContext> DirectxContext) :
	mRef			( Ref ),
	mOpenglContext	( OpenglContext ),
	mDirectxContext	( DirectxContext )
{
	if ( mOpenglContext )
		mOpenglTexturePool.reset( new TPool<Opengl::TTexture>() );
	if ( mDirectxContext )
		mDirectxTexturePool.reset( new TPool<Directx::TTexture>() );

	auto Params = _Params;
	if ( TFileCaster::HandlesFilename( Params.mName ) )
	{
		TCasterDeviceParams DeviceParams;
		DeviceParams.OpenglContext = mOpenglContext;
		DeviceParams.OpenglTexturePool = mOpenglTexturePool;
		DeviceParams.DirectxContext = mDirectxContext;
		DeviceParams.DirectxTexturePool = mDirectxTexturePool;
		mCaster.reset( new TFileCaster( Params, DeviceParams ) );
	}
#if defined(ENABLE_AIRPLAY)
	else if ( Soy::StringTrimLeft( Params.mName, "airplay:", false ) )
	{
		auto& Context = GetAirplayContext();
		mCaster = Context.AllocDevice( Params );
	}
#endif
#if defined(ENABLE_GOOGLECAST)
	else if ( Soy::StringTrimLeft( Params.mName, "chromecast:", false ) )
	{
		auto& Context = GetGoogleCastContext();
		mCaster = Context.AllocDevice( Params );
	}
#endif
	else
	{
		std::stringstream Error;
		Error << "Don't know what caster to make. Try chromecast: airplay: h264:";
		throw Soy::AssertException( Error.str() );
	}
}


void PopCast::TInstance::InitFrameMeta(TCastFrameMeta& Frame,size_t StreamIndex)
{
	SoyTime Now(true);
	if ( !mBaseTimestamp.IsValid() )
	{
		mBaseTimestamp = Now;
	}

	Frame.mStreamIndex = StreamIndex;
	Frame.mTimecode = Now;
	Frame.mTimecode -= mBaseTimestamp;
}


void PopCast::TInstance::WriteFrame(Opengl::TTexture& Texture,size_t StreamIndex)
{
	Soy::Assert( mOpenglContext!=nullptr, "Instance requires an opengl context" );
	Soy::Assert( mCaster != nullptr, "Expected Caster" );
	auto& Context = *mOpenglContext;
	
	//	make relative timestamp
	TCastFrameMeta Frame;
	InitFrameMeta( Frame, StreamIndex );
	
	try
	{
		mCaster->Write( Texture, Frame, Context );
		return;
	}
	catch(std::exception& e)
	{
		//	failed, maybe pixels will be okay
		//std::Debug << "WriteFrame opengl failed: " << e.what() << std::endl;
	}
	
	try
	{
		std::shared_ptr<TCaster> Caster = mCaster;
		auto ReadPixels = [Texture,Caster,Frame]
		{
			std::shared_ptr<SoyPixels> Pixels( new SoyPixels );
			Texture.Read( *Pixels );
			Caster->Write( Pixels, Frame );
		};
		Context.PushJob( ReadPixels );
		return;
	}
	catch(std::exception& e)
	{
		//	failed, maybe pixels will be okay
		//std::Debug << "WriteFrame opengl failed: " << e.what() << std::endl;
	}
	
}


void PopCast::TInstance::WriteFrame(Directx::TTexture& Texture,size_t StreamIndex)
{
#if defined(ENABLE_DIRECTX)
	Soy::Assert( mDirectxContext!=nullptr, "Instance requires an directx context" );
	Soy::Assert( mCaster != nullptr, "Expected Caster" );
	auto& Context = *mDirectxContext;
	
	//	make relative timestamp
	TCastFrameMeta Frame;
	InitFrameMeta( Frame, StreamIndex );
	
	try
	{
		mCaster->Write( Texture, Frame, Context );
		return;
	}
	catch(std::exception& e)
	{
		//	failed, maybe pixels will be okay
		//std::Debug << "WriteFrame opengl failed: " << e.what() << std::endl;
	}
	
	try
	{
		std::shared_ptr<TCaster> Caster = mCaster;
		auto ContextCopy = mDirectxContext;
		auto ReadPixels = [Texture,Caster,Frame,ContextCopy,this]
		{
			std::shared_ptr<SoyPixels> Pixels( new SoyPixels );
			auto& TextureMutable = const_cast<Directx::TTexture&>(Texture);
			TextureMutable.Read( *Pixels, *ContextCopy, mDirectxTexturePool.get() );
			Caster->Write( Pixels, Frame );
		};
		Context.PushJob( ReadPixels );
		return;
	}
	catch(std::exception& e)
	{
		//	failed, maybe pixels will be okay
		//std::Debug << "WriteFrame opengl failed: " << e.what() << std::endl;
	}
#else
	throw Soy::AssertException("Directx not on this platform");
#endif
}

void PopCast::TInstance::WriteFrame(std::shared_ptr<SoyPixelsImpl> Texture,size_t StreamIndex)
{
	Soy::Assert( mCaster != nullptr, "Expected Caster" );
	
	//	make relative timestamp
	TCastFrameMeta Frame;
	InitFrameMeta( Frame, StreamIndex );
	
	try
	{
		mCaster->Write( Texture, Frame );
		return;
	}
	catch(std::exception& e)
	{
		//	failed, maybe pixels will be okay
		//std::Debug << "WriteFrame opengl failed: " << e.what() << std::endl;
	}
	
}

void PopCast::TInstance::GetMeta(TJsonWriter& Json)
{
	if ( mCaster )
	{
		mCaster->GetMeta( Json );
	}
}


size_t PopCast::TInstance::GetPendingPacketCount()
{
	Soy::Assert(mCaster != nullptr, "Caster expected");
	
	return mCaster->GetPendingPacketCount();
}
