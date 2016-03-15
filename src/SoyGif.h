#pragma once

#include <SoyTypes.h>
#include <SoyMedia.h>


class GifWriter;
class GifPalette;
class TRawWriteDataProtocol;

namespace Opengl
{
	class TBlitter;
}

//	https://github.com/ginsweater/gif-h/blob/master/gif.h
namespace Gif
{
	class TMuxer;
	class TEncoder;
	class TEncodeParams;
	
	std::shared_ptr<TMediaEncoder>	AllocEncoder(std::shared_ptr<TMediaPacketBuffer> OutputBuffer,size_t StreamIndex,std::shared_ptr<Opengl::TContext> OpenglContext,const TEncodeParams& Params,bool SkipFrames);

	typedef std::function<bool(const vec3x<uint8>& Old,const vec3x<uint8>& New)>	TMaskPixelFunc;
}

class Gif::TMuxer : public TMediaMuxer
{
public:
	TMuxer(std::shared_ptr<TStreamWriter>& Output,std::shared_ptr<TMediaPacketBuffer>& Input,const std::string& ThreadName);
	~TMuxer();
	
protected:
	virtual void			Finish() override;
	virtual void			SetupStreams(const ArrayBridge<TStreamMeta>&& Streams) override;
	virtual void			ProcessPacket(std::shared_ptr<TMediaPacket> Packet,TStreamWriter& Output) override;

	void					WriteToBuffer(const ArrayBridge<uint8>&& Data);
	void					FlushBuffer();
	
public:
	std::mutex					mBusy;
	std::atomic<bool>			mStarted;
	std::atomic<bool>			mFinished;
	
	std::shared_ptr<SoyPixelsImpl>	mPrevImage;	//	palettised image
};


class TTextureBuffer : public TPixelBuffer
{
public:
	TTextureBuffer(std::shared_ptr<Opengl::TContext> OpenglContext);
	~TTextureBuffer();
	
	virtual void		Lock(ArrayBridge<Opengl::TTexture>&& Textures,Opengl::TContext& Context) override	{}
	virtual void		Lock(ArrayBridge<Directx::TTexture>&& Textures,Directx::TContext& Context) override	{}
	virtual void		Lock(ArrayBridge<SoyPixelsImpl*>&& Textures) override	{}
	virtual void		Unlock() override	{}

	
public:
	std::shared_ptr<Opengl::TContext>	mOpenglContext;
	std::shared_ptr<Opengl::TTexture>	mImage;	//	copy of the image
};

class Gif::TEncodeParams
{
public:
	TEncodeParams() :
		mAllowIntraFrames		( true ),
		mDebugIndexes			( false ),
		mDebugPalette			( false ),
		mDebugTransparency		( false ),
		mFindPalettePixelSkip	( 5 ),
		mTransparentColour		( 255, 0, 255 ),
		mMaxColours				( 255 ),
		mMaskMaxDiff			( 0.f / 256.f )
	{
	}
	
	size_t			mFindPalettePixelSkip;	//	when generating pallete, reduce number of colours we accumualte
	bool			mAllowIntraFrames;		//	use transparency between frames
	bool			mDebugPalette;			//	make a debug palette
	bool			mDebugIndexes;			//	render the pallete top to bottom
	bool			mDebugTransparency;		//	highlight transparent regions
	vec3x<uint8>	mTransparentColour;
	size_t			mMaxColours;
	float			mMaskMaxDiff;			//	if zero, exact pixel colour matches required
};

class Gif::TEncoder : public TMediaEncoder, public SoyWorkerThread
{
public:
	TEncoder(std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex,std::shared_ptr<Opengl::TContext>	OpenglContext,Gif::TEncodeParams Params,bool SkipFrames);
	~TEncoder();
	
	virtual void			Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context) override;
	virtual void			Write(std::shared_ptr<SoyPixelsImpl> Image,SoyTime Timecode) override;

protected:
	virtual bool					CanSleep() override;
	virtual bool					Iteration() override;
	void							PushFrame(std::shared_ptr<TMediaPacket> Frame);
	std::shared_ptr<TMediaPacket>	PopFrame();

	std::shared_ptr<TTextureBuffer>	CopyFrameImmediate(const Opengl::TTexture& Image);
	std::shared_ptr<TTextureBuffer>	CopyFrameImmediate(const SoyPixelsImpl& Image);


	//	if this returns false, we're ALL transparent
	bool					MakePalettisedImage(SoyPixelsImpl& PalettisedImage,const SoyPixelsImpl& Rgba,bool& IsKeyframe,const char* IndexingShader,const TEncodeParams& Params,TMaskPixelFunc MaskPixelFunc);
	void					IndexImageWithShader(SoyPixelsImpl& IndexedImage,const SoyPixelsImpl& Palette,const SoyPixelsImpl& Source,const char* FragShader);
	
public:
	size_t					mPushedFrameCount;
	Gif::TEncodeParams		mParams;
	bool					mSkipFrames;
	size_t					mStreamIndex;
	
	std::mutex				mPendingFramesLock;
	Array<std::shared_ptr<TMediaPacket>>	mPendingFrames;
	
	//	when unity destructs us, the opengl thread is suspended, so we need to forcily break a semaphore
	std::shared_ptr<Soy::TSemaphore>	mOpenglSemaphore;	
	std::shared_ptr<Opengl::TContext>	mOpenglContext;
	std::shared_ptr<Opengl::TBlitter>	mOpenglBlitter;
	
	//	for shader blit
	std::shared_ptr<Opengl::TTexture>	mIndexImage;
	std::shared_ptr<Opengl::TTexture>	mPaletteImage;
	std::shared_ptr<Opengl::TTexture>	mSourceImage;
	
	std::shared_ptr<SoyPixelsImpl>		mPrevRgb;
};
