#include "PopCast.h"
#include "TCaster.h"

#include "TAirplayCaster.h"
#include "PopUnity.h"
#include "TFileCaster.h"

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
};



bool HasBit(Unity::uint ParamBits,TPluginParams::PopCastFlags Param)
{
	auto Masked = (ParamBits & Param);
	return Masked != 0;
}

TCasterParams MakeCasterParams(Unity::uint ParamBits,const char* Filename)
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

	return Params;
}


__export Unity::ulong	PopCast_Alloc(const char* Filename,Unity::uint ParamBits)
{
	ofScopeTimerWarning Timer(__func__, Unity::mMinTimerMs );
	if ( Filename == nullptr )
	{
		std::Debug << "Tried to allocate PopCast with null filename" << std::endl;
		return 0;
	}
	
	auto Params = MakeCasterParams( ParamBits, Filename );

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

__export bool	PopCast_Free(Unity::ulong Instance)
{
	ofScopeTimerWarning Timer(__func__, Unity::mMinTimerMs );
	return PopCast::Free( Instance );
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

__export bool	PopCast_UpdateRenderTexture(Unity::ulong Instance,Unity::NativeTexturePtr TextureId,Unity::sint Width,Unity::sint Height,Unity::RenderTexturePixelFormat::Type PixelFormat,Unity::sint StreamIndex)
{
	auto pInstance = PopCast::GetInstance( Instance );
	if ( !pInstance )
		return false;

	try
	{
		SoyPixelsMeta Meta( Width, Height, Unity::GetPixelFormat( PixelFormat ) );
		auto DirectxContext = Unity::GetDirectxContextPtr();
		auto OpenglContext = Unity::GetOpenglContextPtr();
		
		if ( OpenglContext )
		{
			//	assuming type atm... maybe we can extract it via opengl?
			GLenum Type = GL_TEXTURE_2D;
			Opengl::TTexture Texture( TextureId, Meta, Type );
			pInstance->WriteFrame( Texture, size_cast<size_t>(StreamIndex) );
			return true;
		}

		if ( DirectxContext )
		{
			Directx::TTexture Texture( static_cast<ID3D11Texture2D*>(TextureId) );
			pInstance->WriteFrame( Texture, size_cast<size_t>(StreamIndex) );
			return true;
		}

		throw Soy::AssertException("Missing graphics device");
	}
	catch(std::exception& e)
	{
		std::Debug << __func__ << " failed: " << e.what() << std::endl;
		return false;
	}
}
__export bool	PopCast_UpdateTexture2D(Unity::ulong Instance,Unity::NativeTexturePtr TextureId,Unity::sint Width,Unity::sint Height,Unity::Texture2DPixelFormat::Type PixelFormat,Unity::sint StreamIndex)
{
	auto pInstance = PopCast::GetInstance( Instance );
	if ( !pInstance )
		return false;
	
	try
	{
		SoyPixelsMeta Meta( Width, Height, Unity::GetPixelFormat( PixelFormat ) );
		auto DirectxContext = Unity::GetDirectxContextPtr();
		auto OpenglContext = Unity::GetOpenglContextPtr();
		
		if ( OpenglContext )
		{
			//	assuming type atm... maybe we can extract it via opengl?
			GLenum Type = GL_TEXTURE_2D;
			Opengl::TTexture Texture( TextureId, Meta, Type );
			pInstance->WriteFrame( Texture, size_cast<size_t>(StreamIndex) );
			return true;
		}

		if ( DirectxContext )
		{
			Directx::TTexture Texture( static_cast<ID3D11Texture2D*>(TextureId) );
			pInstance->WriteFrame( Texture, size_cast<size_t>(StreamIndex) );
			return true;
		}

		throw Soy::AssertException("Missing graphics device");
	}
	catch(std::exception& e)
	{
		std::Debug << __func__ << " failed: " << e.what() << std::endl;
		return false;
	}
}

__export bool PopCast_UpdateTextureDebug(Unity::ulong Instance,Unity::sint StreamIndex)
{
	auto pInstance = PopCast::GetInstance( Instance );
	if ( !pInstance )
		return false;
	
	try
	{
		//	make up a debug image
		SoyPixelsMeta Meta( 200, 200, SoyPixelsFormat::RGBA );
		auto pPixels = std::make_shared<SoyPixels>();
		auto& Pixels = *pPixels;
		Pixels.Init( Meta );
		BufferArray<uint8,4> Rgba;
		Rgba.PushBack( 255 );
		Rgba.PushBack( 0 );
		Rgba.PushBack( 0 );
		Rgba.PushBack( 255 );
		Pixels.SetPixels( GetArrayBridge(Rgba) );

		pInstance->WriteFrame( pPixels, size_cast<size_t>(StreamIndex) );
		return true;
	}
	catch(std::exception& e)
	{
		std::Debug << __func__ << " failed: " << e.what() << std::endl;
		return false;
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

	if ( !DirectxContext )
		DirectxContext = Unity::GetDirectxContextPtr();
	
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



PopCast::TInstance::TInstance(const TInstanceRef& Ref,TCasterParams& _Params,std::shared_ptr<Opengl::TContext> OpenglContext,std::shared_ptr<Directx::TContext> DirectxContext) :
	mRef			( Ref ),
	mOpenglContext	( OpenglContext ),
	mDirectxContext	( DirectxContext ),
	mBaseTimestamp	( true )	//	timestamps based on now
{
	auto Params = _Params;
	if ( TFileCaster::HandlesFilename( Params.mName ) )
	{
		mCaster.reset( new TFileCaster( Params, mOpenglContext, mDirectxContext ) );
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


void PopCast::TInstance::WriteFrame(Opengl::TTexture& Texture,size_t StreamIndex)
{
	Soy::Assert( mOpenglContext!=nullptr, "Instance requires an opengl context" );
	Soy::Assert( mCaster != nullptr, "Expected Caster" );
	auto& Context = *mOpenglContext;
	
	//	make relative timestamp
	TCastFrameMeta Frame;
	Frame.mStreamIndex = StreamIndex;
	Frame.mTimecode = SoyTime(true);
	Frame.mTimecode -= mBaseTimestamp;
	
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
	Soy::Assert( mDirectxContext!=nullptr, "Instance requires an opengl context" );
	Soy::Assert( mCaster != nullptr, "Expected Caster" );
	auto& Context = *mDirectxContext;
	
	//	make relative timestamp
	TCastFrameMeta Frame;
	Frame.mStreamIndex = StreamIndex;
	Frame.mTimecode = SoyTime(true);
	Frame.mTimecode -= mBaseTimestamp;
	
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
			TextureMutable.Read( *Pixels, *ContextCopy, mDirectxTexturePool );
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

void PopCast::TInstance::WriteFrame(std::shared_ptr<SoyPixelsImpl> Texture,size_t StreamIndex)
{
	Soy::Assert( mCaster != nullptr, "Expected Caster" );
	
	//	make relative timestamp
	TCastFrameMeta Frame;
	Frame.mStreamIndex = StreamIndex;
	Frame.mTimecode = SoyTime(true);
	Frame.mTimecode -= mBaseTimestamp;
	
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