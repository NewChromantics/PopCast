#include "PopCast.h"
#include "TCaster.h"
#include "TGoogleCaster.h"



namespace PopCast
{
	std::mutex gInstancesLock;
	std::vector<std::shared_ptr<PopCast::TInstance> >	gInstances;
};


__export Unity::ulong	PopCast_Alloc(const char* Filename,TPopCastParams Params)
{
	ofScopeTimerWarning Timer(__func__, Unity::gParams.mMinTimerMs );
	if ( Filename == nullptr )
	{
		std::Debug << "Tried to allocate PopCast with null filename" << std::endl;
		return 0;
	}
	
	Unity::gParams.mDebugPluginEvent = bool_cast(Params.mDebugRenderThreadCallback);
	

	try
	{
		auto Instance = PopCast::Alloc( Params );
		if ( !Instance )
			return 0;
		return Instance->GetRef().GetInt64();
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
	return PopCast::Free( PopCast::TInstanceRef( Instance ) );
}

__export void	PopCast_EnumDevices()
{
	Array<TCastDeviceMeta> Metas;
	GoogleCast::EnumDevices( GetArrayBridge( Metas ) );
	
	for ( int i=0;	i<Metas.GetSize();	i++ )
	{
		std::Debug << Metas[i].mName << std::endl;
	}
}


std::shared_ptr<PopCast::TInstance> PopCast::Alloc(const TPopCastParams& Params)
{
	gInstancesLock.lock();
	static TInstanceRef gInstanceRefCounter(1000);
	gInstanceRefCounter.mRef++;
	gInstancesLock.unlock();

	//	try to alloc, if this fails it'll throw
	std::shared_ptr<TInstance> pInstance( new TInstance(gInstanceRefCounter,Params) );
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



PopCast::TInstance::TInstance(const TInstanceRef& Ref,TPopCastParams Params) :
	mRef		( Ref )
{
	mCaster.reset( new TGoogleCaster() );

}

