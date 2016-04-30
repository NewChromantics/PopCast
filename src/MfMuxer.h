#pragma once

#include "TCaster.h"
#include <SoyMedia.h>
#include <SoyMediaFoundation.h>
#include "TBlitterDirectx.h"

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

	size_t		CreateVideoStream(SoyMediaFormat::Type Format,size_t FrameRate,size_t BitRate,SoyPixelsMeta InputMeta,size_t StreamIndex);	//	returns output stream index

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
	std::shared_ptr<TContext>		mMediaFoundationContext;

	std::map<size_t,size_t>			mInputToOutputStreamIndex;
	std::function<void(bool&)>		mOnStreamFinished;
	std::shared_ptr<TSinkWriter>	mSinkWriter;

	bool							mStarted;
	bool							mFinished;
	std::mutex						mWriteLock;
};


class MfEncoder : public TMediaEncoder
{
public:
	MfEncoder(std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex,SoyPixelsMeta OutputMeta);

	virtual void		Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context) override;
	virtual void		Write(const Directx::TTexture& Image,SoyTime Timecode,Directx::TContext& Context) override;
	virtual void		Write(std::shared_ptr<SoyPixelsImpl> Image,SoyTime Timecode) override;

	void				OnError(const std::string& Error);

private:
	TPool<Directx::TTexture>&	GetDirectxTexturePool();
	Directx::TBlitter&			GetDirectxBlitter();
	void						PushPixelBufferFrame(std::shared_ptr<TPixelBuffer>& PixelBuffer,SoyTime Timecode,SoyPixelsMeta PixelMeta);

public:
	std::shared_ptr<Directx::TBlitter>	mDirectxBlitter;
	std::shared_ptr<TPool<Directx::TTexture>>	mDirectxTexturePool;
	SoyPixelsMeta				mOutputMeta;
	TMediaPacketBuffer			mFrames;
	size_t						mStreamIndex;	//	may want to be in base
};


