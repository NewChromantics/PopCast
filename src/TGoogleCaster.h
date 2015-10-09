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
	
	class TDevice;
	class TDeviceInternal;
}


class TMediaMeta
{
public:
	std::string		mTitle;
	std::string		mSubtitle;
	std::string		mImageUrl;
	size_t			mImageWidth;
	size_t			mImageHeight;
	std::string		mMediaUrl;		//	"http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4"
	size_t			mDuration;
	std::string		mContentType;	//	"video/mp4"
	size_t			mStartTime;
	bool			mAutoPlay;
};



class GoogleCast::TContext
{
	friend class TContextInternal;
public:
	TContext(const std::string& AppName="CBFF3EA9");
	~TContext();
	
	std::shared_ptr<TDevice>	AllocDevice(TCasterParams Params);
	virtual void				EnumDevices(ArrayBridge<TCastDeviceMeta>&& Metas);

protected:
	std::string					mAppName;
	
private:
	std::shared_ptr<TContextInternal>		mInternal;
	std::shared_ptr<Soy::TRuntimeLibrary>	mLibrary;
};


class GoogleCast::TDevice : public TCaster
{
public:
	TDevice(void* GCKDevice);
	
	virtual void		Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context) override;
	virtual void		Write(const std::shared_ptr<SoyPixelsImpl> Image,SoyTime Timecode) override;

protected:
	void		Connect();
	void		LoadMedia(const TMediaMeta& Media);
	
public:
	std::shared_ptr<TDeviceInternal>	mInternal;
};


