#pragma once

#include "TCaster.h"
#include <SoyMedia.h>


namespace Avf
{
	class TFileMuxer;		//	asset writer
	class TAssetWriterInput;
	class TAssetWriter;
};



class Avf::TFileMuxer : public TMediaMuxer
{
public:
	TFileMuxer(const std::string& Filename,std::shared_ptr<TMediaPacketBuffer>& Input,const std::function<void(bool&)>& OnStreamFinished);
	
protected:
	virtual void	SetupStreams(const ArrayBridge<TStreamMeta>&& Streams) override;
	virtual void	ProcessPacket(std::shared_ptr<TMediaPacket> Packet,TStreamWriter& Output) override;
	virtual void	Finish() override;
	
	void			OnPreWrite(SoyTime Timecode);
	
public:
	std::map<size_t,std::shared_ptr<TAssetWriterInput>>	mStreamWriters;
	std::shared_ptr<TAssetWriter>	mAssetWriter;
	SoyTime			mLastTimecode;
	SoyTime			mFirstTimecode;
	std::function<void(bool&)>		mOnStreamFinished;
	bool			mDebugWriting;
};

