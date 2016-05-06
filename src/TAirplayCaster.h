#pragma once

#include "TCaster.h"
#include "SoyHttpConnection.h"
#include "SoyMulticast.h"
#include <SoySocketStream.h>


namespace Airplay
{
	class TContext;
	class TContextInternal;	//	objective-c classes
	
	class TMirrorDevice;
	typedef TMirrorDevice TDevice;
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


class Airplay::TMirrorDevice : public TCaster
{
public:
	TMirrorDevice(const std::string& Name,const std::string& Address,const TCasterParams& Params);
	
	virtual void		Write(std::shared_ptr<TPixelBuffer>& PixelBuffer,const TCastFrameMeta& Frame,std::shared_ptr<Opengl::TContext>& OpenglContext,std::shared_ptr<Directx::TContext>& DirectxContext) override;

protected:
	bool				mSentHeader;	//	when sent, we're just sending video stream
	std::string			mServerXml;
	
public:
	std::shared_ptr<THttpConnection>	mConnection;
	std::shared_ptr<TDeviceInternal>	mInternal;
};


