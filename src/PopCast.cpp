#include "PopCast.h"
#include "TCaster.h"
#include "TGoogleCaster.h"



namespace PopCast
{
	std::mutex gInstancesLock;
	std::vector<std::shared_ptr<PopCast::TInstance> >	gInstances;
	
	std::shared_ptr<GoogleCast::TContext>	GoogleCastContext;
	GoogleCast::TContext&					GetGoogleCastContext();
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
	
	for ( int i=0;	i<Metas.GetSize();	i++ )
	{
		std::Debug << Metas[i] << std::endl;
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
	mRef		( Ref )
{
	//	try to alloc, if this fails it'll throw
	if ( Soy::StringTrimLeft( Params.mName, "chromecast:", false ) )
	{
		auto& Context = GetGoogleCastContext();
		mCaster = Context.AllocDevice( Params );
	}
	else
	{
		std::stringstream Error;
		Error << "Don't know what caster to make. Try chromecast:";
		throw Soy::AssertException( Error.str() );
	}
}

