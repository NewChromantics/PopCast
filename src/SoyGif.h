#pragma once

#include <SoyTypes.h>
#include <SoyMedia.h>


class GifWriter;
class GifPalette;
class TRawWriteDataProtocol;

template<typename TYPE>
class TPool;


namespace Opengl
{
	class TBlitter;
	class GifBlitter;
}
namespace Directx
{
	class TBlitter;
	class GifBlitter;
}

//	https://github.com/ginsweater/gif-h/blob/master/gif.h
namespace Gif
{
	class TMuxer;
	class TEncoder;
	class TEncodeParams;
	
	std::shared_ptr<TMediaEncoder>	AllocEncoder(std::shared_ptr<TMediaPacketBuffer> OutputBuffer,size_t StreamIndex,std::shared_ptr<Opengl::TContext> Context,std::shared_ptr<TPool<Opengl::TTexture>> TexturePool,const TEncodeParams& Params,bool SkipFrames);
	std::shared_ptr<TMediaEncoder>	AllocEncoder(std::shared_ptr<TMediaPacketBuffer> OutputBuffer,size_t StreamIndex,std::shared_ptr<Directx::TContext> Context,std::shared_ptr<TPool<Directx::TTexture>> TexturePool,const TEncodeParams& Params,bool SkipFrames);

	typedef std::function<bool(const vec3x<uint8>& Old,const vec3x<uint8>& New)>	TMaskPixelFunc;
}


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
		mMaskMaxDiff			( 0.f / 256.f ),
		mCpuOnly				( false ),
		mLzwCompression			( true )
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
	bool			mCpuOnly;				//	no gpu stuff. good for debugging muxer etc
	bool			mLzwCompression;
};



class Gif::TMuxer : public TMediaMuxer
{
public:
	TMuxer(std::shared_ptr<TStreamWriter>& Output,std::shared_ptr<TMediaPacketBuffer>& Input,const std::string& ThreadName,const TEncodeParams& Params);
	~TMuxer();
	
protected:
	virtual void			Finish() override;
	virtual void			SetupStreams(const ArrayBridge<TStreamMeta>&& Streams) override;
	virtual void			ProcessPacket(std::shared_ptr<TMediaPacket> Packet,TStreamWriter& Output) override;

	void					WriteToBuffer(const ArrayBridge<uint8>&& Data);
	void					FlushBuffer();
	
public:
	bool						mLzwCompression;
	std::mutex					mBusy;
	std::atomic<bool>			mStarted;
	std::atomic<bool>			mFinished;
	
	std::shared_ptr<SoyPixelsImpl>	mPrevImage;	//	palettised image
};


class TTextureBuffer : public TPixelBuffer
{
public:
	TTextureBuffer()	{}
	TTextureBuffer(std::shared_ptr<Opengl::TContext> Context);
	TTextureBuffer(std::shared_ptr<Directx::TContext> Context);
	TTextureBuffer(std::shared_ptr<SoyPixelsImpl> Pixels);
	~TTextureBuffer();
	
	virtual void		Lock(ArrayBridge<Opengl::TTexture>&& Textures,Opengl::TContext& Context) override	{}
	virtual void		Lock(ArrayBridge<Directx::TTexture>&& Textures,Directx::TContext& Context) override	{}
	virtual void		Lock(ArrayBridge<SoyPixelsImpl*>&& Textures) override	{}
	virtual void		Unlock() override	{}

	
public:
	std::shared_ptr<SoyPixelsImpl>		mPixels;

	std::shared_ptr<Opengl::TContext>	mOpenglContext;
	std::shared_ptr<Opengl::TTexture>	mOpenglTexture;

	std::shared_ptr<Directx::TContext>	mDirectxContext;
	std::shared_ptr<Directx::TTexture>	mDirectxTexture;
};


class TGifBlitter
{
public:
	virtual std::shared_ptr<SoyPixelsImpl>	IndexImageWithShader(std::shared_ptr<SoyPixelsImpl> Palette,std::shared_ptr<SoyPixelsImpl> Source,const char* FragShader,std::shared_ptr<Soy::TSemaphore> JobSempahore)=0;

public:
};


//	software version
class TCpuGifBlitter : public TGifBlitter
{
public:
	virtual std::shared_ptr<SoyPixelsImpl>	IndexImageWithShader(std::shared_ptr<SoyPixelsImpl> Palette,std::shared_ptr<SoyPixelsImpl> Source,const char* FragShader,std::shared_ptr<Soy::TSemaphore> JobSempahore) override;

public:
};

class Opengl::GifBlitter : public TGifBlitter
{
public:
	GifBlitter(std::shared_ptr<TContext> Context,std::shared_ptr<TPool<TTexture>> TexturePool);
	~GifBlitter();

	std::shared_ptr<TTextureBuffer>			CopyImmediate(const TTexture& Image);
	virtual std::shared_ptr<SoyPixelsImpl>	IndexImageWithShader(std::shared_ptr<SoyPixelsImpl> Palette,std::shared_ptr<SoyPixelsImpl> Source,const char* FragShader,std::shared_ptr<Soy::TSemaphore> JobSempahore) override;
	TPool<TTexture>&						GetTexturePool()	{	return *mTexturePool;	}

public:
	std::shared_ptr<TContext>			mContext;
	std::shared_ptr<TBlitter>			mBlitter;
	std::shared_ptr<TPool<TTexture>>	mTexturePool;
};


class Directx::GifBlitter : public TGifBlitter
{
public:
	GifBlitter(std::shared_ptr<TContext> Context,std::shared_ptr<TPool<TTexture>> TexturePool);
	~GifBlitter();

	std::shared_ptr<TTextureBuffer>			CopyImmediate(const TTexture& Image);
	virtual std::shared_ptr<SoyPixelsImpl>	IndexImageWithShader(std::shared_ptr<SoyPixelsImpl> Palette,std::shared_ptr<SoyPixelsImpl> Source,const char* FragShader,std::shared_ptr<Soy::TSemaphore> JobSempahore) override;
	TPool<TTexture>&						GetTexturePool()	{	return *mTexturePool;	}

public:
	std::shared_ptr<TContext>			mContext;
	std::shared_ptr<TBlitter>			mBlitter;
	std::shared_ptr<TPool<TTexture>>	mTexturePool;
};



class Gif::TEncoder : public TMediaEncoder, public SoyWorkerThread
{
public:
	TEncoder(std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex,std::shared_ptr<Opengl::TContext> Context,std::shared_ptr<TPool<Opengl::TTexture>> TexturePool,TEncodeParams Params,bool SkipFrames);
	TEncoder(std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex,std::shared_ptr<Directx::TContext> Context,std::shared_ptr<TPool<Directx::TTexture>> TexturePool,TEncodeParams Params,bool SkipFrames);
	TEncoder(std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex,TEncodeParams Params,bool SkipFrames);
	~TEncoder();
	
	virtual void			Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context) override;
	virtual void			Write(const Directx::TTexture& Image,SoyTime Timecode,Directx::TContext& Context) override;
	virtual void			Write(std::shared_ptr<SoyPixelsImpl> Image,SoyTime Timecode) override;

protected:
	virtual bool					CanSleep() override;
	virtual bool					Iteration() override;
	void							PushFrame(std::shared_ptr<TMediaPacket> Frame);
	std::shared_ptr<TMediaPacket>	PopFrame();

	Opengl::TContext&		GetOpenglContext();
	Directx::TContext&		GetDirectxContext();

	//	if this returns false, we're ALL transparent
	bool					MakePalettisedImage(SoyPixelsImpl& PalettisedImage,const SoyPixelsImpl& Rgba,bool& IsKeyframe,const char* IndexingShader,const TEncodeParams& Params,TMaskPixelFunc MaskPixelFunc);
	std::shared_ptr<SoyPixelsImpl>	IndexImageWithShader(std::shared_ptr<SoyPixelsImpl> Palette,std::shared_ptr<SoyPixelsImpl> Source,const char* FragShader);
	
	bool					CanPushFrame(SoyTime Timecode);
	void					OnFramePrePushSkipped(SoyTime Timcode);

private:
	std::shared_ptr<Soy::TSemaphore>	AllocJobSempahore();
	void								AbortJobSemaphores();

public:
	size_t					mPushedFrameCount;
	Gif::TEncodeParams		mParams;
	bool					mSkipFrames;
	size_t					mStreamIndex;
	
	std::mutex				mPendingFramesLock;
	Array<std::shared_ptr<TMediaPacket>>	mPendingFrames;
	

	std::shared_ptr<Opengl::GifBlitter>		mOpenglGifBlitter;
	std::shared_ptr<Directx::GifBlitter>	mDirectxGifBlitter;
	std::shared_ptr<TGifBlitter>			mCpuGifBlitter;

	std::shared_ptr<SoyPixelsImpl>			mPrevRgb;

private:
	//	when unity destructs us, the opengl thread is suspended, so we need to forcily break a semaphore
	//	gr: this gets messy, work on it
	std::mutex								mJobSemaphoresLock;
	Array<std::shared_ptr<Soy::TSemaphore>>	mJobSemaphores;

};
