#include "PopCast.h"
#include "TCaster.h"
#include "TGoogleCaster.h"
#include "TAirplayCaster.h"
#include "PopUnity.h"
#include "TFileCaster.h"

#if defined(TARGET_OSX)||defined(TARGET_IOS)
#include "AvfCompressor.h"
#endif


namespace PopCast
{
	std::mutex gInstancesLock;
	std::vector<std::shared_ptr<PopCast::TInstance> >	gInstances;
	
	std::shared_ptr<GoogleCast::TContext>	GoogleCastContext;
	GoogleCast::TContext&					GetGoogleCastContext();
	std::shared_ptr<Airplay::TContext>		AirplayContext;
	Airplay::TContext&						GetAirplayContext();
};





__export Unity::ulong	PopCast_Alloc(const char* Filename)
{
	ofScopeTimerWarning Timer(__func__, Unity::gParams.mMinTimerMs );
	if ( Filename == nullptr )
	{
		std::Debug << "Tried to allocate PopCast with null filename" << std::endl;
		return 0;
	}
	
	TCasterParams Params;
	Params.mName = Filename;
	

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
	ofScopeTimerWarning Timer(__func__, Unity::gParams.mMinTimerMs );
	return PopCast::Free( Instance );
}

__export void	PopCast_EnumDevices()
{
	Array<TCastDeviceMeta> Metas;

	//	get chromecast devices
	try
	{
		auto& GoogleCastContext = PopCast::GetGoogleCastContext();
		GoogleCastContext.EnumDevices( GetArrayBridge( Metas ) );
	}
	catch (std::exception& e)
	{
		std::Debug << e.what() << std::endl;
	}
	
	try
	{
		auto& AirplayContext = PopCast::GetAirplayContext();
		AirplayContext.EnumDevices( GetArrayBridge( Metas ) );
	}
	catch (std::exception& e)
	{
		std::Debug << e.what() << std::endl;
	}
	
	for ( int i=0;	i<Metas.GetSize();	i++ )
	{
		std::Debug << Metas[i] << std::endl;
	}
}

__export bool	PopCast_UpdateRenderTexture(Unity::ulong Instance,Unity::NativeTexturePtr TextureId,Unity::sint Width,Unity::sint Height,Unity::RenderTexturePixelFormat::Type PixelFormat)
{
	auto pInstance = PopCast::GetInstance( Instance );
	if ( !pInstance )
		return false;

	try
	{
		auto& Context = Unity::GetOpenglContext();
		
		//	assuming type atm... maybe we can extract it via opengl?
		SoyPixelsMeta Meta( Width, Height, Unity::GetPixelFormat( PixelFormat ) );
		GLenum Type = GL_TEXTURE_2D;
		Opengl::TTexture Texture( TextureId, Meta, Type );
		pInstance->WriteFrame( Texture, Context );
		return true;
	}
	catch(std::exception& e)
	{
		std::Debug << __func__ << " failed: " << e.what() << std::endl;
		return false;
	}
}
__export bool	PopCast_UpdateTexture2D(Unity::ulong Instance,Unity::NativeTexturePtr TextureId,Unity::sint Width,Unity::sint Height,Unity::Texture2DPixelFormat::Type PixelFormat)
{
	auto pInstance = PopCast::GetInstance( Instance );
	if ( !pInstance )
		return false;
	
	try
	{
		auto& Context = Unity::GetOpenglContext();

		//	assuming type atm... maybe we can extract it via opengl?
		SoyPixelsMeta Meta( Width, Height, Unity::GetPixelFormat( PixelFormat ) );
		GLenum Type = GL_TEXTURE_2D;
		Opengl::TTexture Texture( TextureId, Meta, Type );
		pInstance->WriteFrame( Texture, Context );
		return true;
	}
	catch(std::exception& e)
	{
		std::Debug << __func__ << " failed: " << e.what() << std::endl;
		return false;
	}
}


GoogleCast::TContext& PopCast::GetGoogleCastContext()
{
	if ( !PopCast::GoogleCastContext )
	{
		PopCast::GoogleCastContext.reset( new GoogleCast::TContext() );
	}
	
	return *PopCast::GoogleCastContext;
}



Airplay::TContext& PopCast::GetAirplayContext()
{
	if ( !PopCast::AirplayContext )
	{
		PopCast::AirplayContext.reset( new Airplay::TContext() );
	}
	
	return *PopCast::AirplayContext;
}



std::shared_ptr<PopCast::TInstance> PopCast::Alloc(TCasterParams Params)
{
	gInstancesLock.lock();
	static TInstanceRef gInstanceRefCounter(1000);
	auto InstanceRef = gInstanceRefCounter++;
	gInstancesLock.unlock();

	std::shared_ptr<TInstance> pInstance( new TInstance(InstanceRef,Params) );
	
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



PopCast::TInstance::TInstance(const TInstanceRef& Ref,TCasterParams Params) :
	mRef			( Ref ),
	mBaseTimestamp	( true )	//	timestamps based on now
{
	
	if ( Soy::StringTrimLeft( Params.mName, "airplay:", false ) )
	{
		auto& Context = GetAirplayContext();
		mCaster = Context.AllocDevice( Params );
	}
	else if ( Soy::StringTrimLeft( Params.mName, "chromecast:", false ) )
	{
		auto& Context = GetGoogleCastContext();
		mCaster = Context.AllocDevice( Params );
	}
	else if ( Soy::StringTrimLeft( Params.mName, "file:", false ) )
	{
		mCaster.reset( new TFileCaster(Params) );
	}
	else
	{
		std::stringstream Error;
		Error << "Don't know what caster to make. Try chromecast: airplay: h264:";
		throw Soy::AssertException( Error.str() );
	}
}

void PopCast::TInstance::WriteFrame(Opengl::TTexture Texture,Opengl::TContext& Context)
{
	Soy::Assert( mCaster != nullptr, "Expected Caster" );
	
	//	make relative timestamp
	SoyTime Timestamp(true);
	Timestamp -= mBaseTimestamp;
	
	try
	{
		mCaster->Write( Texture, Timestamp, Context );
		return;
	}
	catch(std::exception& e)
	{
		//	failed, maybe pixels will be okay
		std::Debug << "WriteFrame opengl failed: " << e.what() << std::endl;
	}
	
	try
	{
		std::shared_ptr<TCaster> Caster = mCaster;
		auto ReadPixels = [Texture,Caster,Timestamp]
		{
			std::shared_ptr<SoyPixels> Pixels( new SoyPixels );
			Texture.Read( *Pixels );
			Caster->Write( Pixels, Timestamp );
		};
		Context.PushJob( ReadPixels );
		return;
	}
	catch(std::exception& e)
	{
		//	failed, maybe pixels will be okay
		std::Debug << "WriteFrame opengl failed: " << e.what() << std::endl;
	}
	
}
