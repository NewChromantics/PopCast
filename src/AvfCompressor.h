#pragma once

#include "TCaster.h"

namespace AvfCompressor
{
	class TInstance;
	class TSession;		//	objc interface
	
	
	std::shared_ptr<TInstance>	Allocate(const TCasterParams& Params);
};



class TFrameMeta
{
public:
	TFrameMeta() :
		mKeyframe	( false )
	{
	}
	
public:
	SoyTime		mTime;
	bool		mKeyframe;
};

class TH264Frame
{
public:
	TFrameMeta		mMeta;
	Array<uint8>	mFrame;
};


//	http://asciiwwdc.com/2014/sessions/513
class AvfCompressor::TInstance : public TCaster
{
public:
	TInstance(const TCasterParams& Params);
	
	virtual void		Write(const Opengl::TTexture& Image,SoyTime Timecode) override;
	virtual void		Write(const SoyPixelsImpl& Image,SoyTime Timecode) override;

	void				PushCompressedFrame(std::shared_ptr<TH264Frame> Frame);
	void				OnError(const std::string& Error);

public:
	std::shared_ptr<TSession>	mSession;
	SoyPixelsMeta				mOutputMeta;
	Array<std::shared_ptr<TH264Frame>>	mFrames;
};


