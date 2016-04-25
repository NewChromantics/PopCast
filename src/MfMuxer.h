#pragma once

#include "TCaster.h"
#include <SoyMedia.h>


namespace MediaFoundation
{
	class TFileMuxer;		//	asset writer
	class TAssetWriterInput;
	class TAssetWriter;
};

class IMFSinkWriter;


class TSinkWriter
{
public:
	TSinkWriter(const std::string& Filename);

	size_t		CreateVideoStream(SoyMediaFormat::Type Format,size_t FrameRate,size_t BitRate,SoyPixelsMeta InputMeta);	//	returns output stream index

public:
	AutoReleasePtr<IMFSinkWriter>	mSinkWriter;
};

class MediaFoundation::TFileMuxer : public TMediaMuxer
{
public:
	TFileMuxer(const std::string& Filename,std::shared_ptr<TMediaPacketBuffer>& Input,const std::function<void(bool&)>& OnStreamFinished);
	
protected:
	virtual void	SetupStreams(const ArrayBridge<TStreamMeta>&& Streams) override;
	virtual void	ProcessPacket(std::shared_ptr<TMediaPacket> Packet,TStreamWriter& Output) override;
	virtual void	Finish() override;
	
	size_t			GetOutputStream(size_t InputStreamIndex);	//	throws if we have no stream assigned

public:
	std::map<size_t,size_t>			mInputToOutputStreamIndex;
	std::function<void(bool&)>		mOnStreamFinished;
	std::shared_ptr<TSinkWriter>	mSinkWriter;

	bool							mStarted;
	bool							mFinished;
	std::mutex						mWriteLock;
};

