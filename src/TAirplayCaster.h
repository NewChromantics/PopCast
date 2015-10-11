#pragma once

#include "TCaster.h"
#include "SoyHttpConnection.h"
#include "SoyMulticast.h"


namespace Airplay
{
	class TContext;
	class TContextInternal;	//	objective-c classes
	
	class TDevice;
	class TDeviceInternal;
}




class Airplay::TContext
{
	friend class TContextInternal;
public:
	TContext();
	~TContext();
	
	std::shared_ptr<TDevice>	AllocDevice(TCasterParams Params);
	virtual void				EnumDevices(ArrayBridge<TCastDeviceMeta>&& Metas);

	
private:
	std::shared_ptr<SoyMulticaster>			mMulticaster;
	std::shared_ptr<TContextInternal>		mInternal;
};


class Airplay::TDevice : public TCaster
{
public:
	TDevice(const TCasterParams& Params);
	
	virtual void		Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context) override;
	virtual void		Write(const std::shared_ptr<SoyPixelsImpl> Image,SoyTime Timecode) override;

protected:
	
public:
	std::shared_ptr<THttpConnection>	mConnection;
	std::shared_ptr<THttpConnection>	mReverseConnection;
	std::shared_ptr<TDeviceInternal>	mInternal;
};


