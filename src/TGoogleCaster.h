#pragma once

#include "TCaster.h"


namespace Soy
{
	class TRuntimeLibrary;
}

namespace GoogleCast
{
	class TContext;
	class TContextInternal;	//	objective-c classes
}


class GoogleCast::TContext
{
	friend class TContextInternal;
public:
	TContext(const std::string& AppName="CBFF3EA9");
	~TContext();
	
	virtual void		EnumDevices(ArrayBridge<TCastDeviceMeta>&& Metas);

protected:
	std::string					mAppName;
	std::mutex					mDevicesLock;
	Array<TCastDeviceMeta>		mDevices;
	
private:
	std::shared_ptr<TContextInternal>		mInternal;
	std::shared_ptr<Soy::TRuntimeLibrary>	mLibrary;
};


