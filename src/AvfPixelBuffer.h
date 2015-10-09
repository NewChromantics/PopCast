#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <map>

#include "PopUnity.h"
#include <SoyEvent.h>
#include <SoyThread.h>

#if defined(__OBJC__)
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#endif

class AvfMovieDecoder;
class AvfDecoderRenderer;
class AvfTextureCache;

namespace Directx
{
	class TContext;
	class TTexture;
}

namespace Soy
{
	namespace Platform
	{
		std::string		GetCVReturnString(CVReturn Error);
		std::string		GetCodec(CMFormatDescriptionRef FormatDescription);
		std::string		GetExtensions(CMFormatDescriptionRef FormatDescription);
	}
}



class TPixelBuffer
{
public:
	//	different paths return arrays now - shader/fbo blit is pretty generic now so move it out of pixel buffer
	//	generic array, handle that internally (each implementation tends to have it's own lock info anyway)
	//	for future devices (metal, dx), expand these
	//	if 1 texture assume RGB/BGR greyscale etc
	//	if multiple, assuming YUV
	//virtual void		Lock(ArrayBridge<Metal::TTexture>&& Textures)=0;
	//virtual void		Lock(ArrayBridge<Cuda::TTexture>&& Textures)=0;
	//virtual void		Lock(ArrayBridge<Opencl::TTexture>&& Textures)=0;
	virtual void		Lock(ArrayBridge<Opengl::TTexture>&& Textures,Opengl::TContext& Context)=0;
	virtual void		Lock(ArrayBridge<Directx::TTexture>&& Textures,Directx::TContext& Context)=0;
	virtual void		Lock(ArrayBridge<SoyPixelsImpl*>&& Textures)=0;
	virtual void		Unlock()=0;
};




#if defined(__OBJC__)
class AvfTextureCache
{
public:
	AvfTextureCache();
	~AvfTextureCache();
	
	void				Flush();
	
#if defined(TARGET_IOS)
	CFPtr<CVOpenGLESTextureCacheRef>	mTextureCache;
#elif defined(TARGET_OSX)
	CFPtr<CVOpenGLTextureCacheRef>		mTextureCache;
#endif
};
#endif



class AvfDecoderRenderer
{
public:
	AvfDecoderRenderer()
	{
	}
	~AvfDecoderRenderer()
	{
		mTextureCaches.Clear();
	}
	
	std::shared_ptr<AvfTextureCache>	GetTextureCache(size_t Index);
	
public:
	Array<std::shared_ptr<AvfTextureCache>>	mTextureCaches;
};



#if defined(__OBJC__)
class CFPixelBuffer : public TPixelBuffer
{
public:
	CFPixelBuffer(CMSampleBufferRef Buffer,bool DoRetain,SoyPixelsFormat::Type Format,std::shared_ptr<AvfDecoderRenderer>& Decoder) :
		mFormat			( Format ),
		mSample			( Buffer, DoRetain ),
		mDecoder		( Decoder ),
		mLockedPixels	( 2 ),
		mReadOnlyLock	( true )
	{
		if ( !Soy::Assert( mSample, "Sample expected") )
			return;
		
		//auto RetainCount = mSample.GetRetainCount();
		//std::Debug << "CFPixelBuffer constructor; sample has " << RetainCount << " references in " << __func__ << std::endl;
	}
	~CFPixelBuffer();

	virtual void			Lock(ArrayBridge<Directx::TTexture>&& Textures,Directx::TContext& Context) override		{}
	virtual void			Lock(ArrayBridge<Opengl::TTexture>&& Textures,Opengl::TContext& Context) override;
	virtual void			Lock(ArrayBridge<SoyPixelsImpl*>&& Textures) override;
	virtual void			Unlock() override;

	void					WaitForUnlock();
	
private:
	CVImageBufferRef		LockImageBuffer();
	
private:
	bool						mReadOnlyLock;
	SoyPixelsFormat::Type		mFormat;
	std::shared_ptr<AvfDecoderRenderer>		mDecoder;
	CFPtr<CMSampleBufferRef>	mSample;

	CFPtr<CVImageBufferRef>		mLockedImageBuffer;	//	this has been [retained] for safety
	std::mutex					mLockLock;
	
	Array<std::shared_ptr<AvfTextureCache>>	mTextureCaches;
#if defined(TARGET_IOS)
	//	ios has 2 texture caches for multiple planes. Just 0 is used for non-planar
	CFPtr<CVOpenGLESTextureRef>			mLockedTexture0;
	CFPtr<CVOpenGLESTextureRef>			mLockedTexture1;
#elif defined(TARGET_OSX)
	CFPtr<CVOpenGLTextureRef>			mLockedTexture;
#endif
	BufferArray<SoyPixelsRemote,2>		mLockedPixels;
};
#endif

