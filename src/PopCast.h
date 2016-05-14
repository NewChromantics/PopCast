#pragma once

#include "PopUnity.h"
#include "TCaster.h"


#if defined(ENABLE_DIRECTX)
//	needed for the pool
#include <SoyDirectx.h>	
#endif
#include <SoyPool.h>

namespace Directx
{
	class TContext;
	class TTexture;
}

class TCaster;
class TCasterParams;
class TJsonWriter;

__export Unity::uint	PopCast_Alloc(const char* Filename,Unity::uint ParamBits,Unity::Float RateMegaBytesPerSec);
__export bool			PopCast_Free(Unity::uint Instance);
__export void			PopCast_EnumDevices();
__export bool			PopCast_UpdateRenderTexture(Unity::uint Instance,Unity::NativeTexturePtr TextureId,Unity::sint Width,Unity::sint Height,Unity::RenderTexturePixelFormat::Type PixelFormat,Unity::sint StreamIndex);
__export bool			PopCast_UpdateTexture2D(Unity::uint Instance,Unity::NativeTexturePtr TextureId,Unity::sint Width,Unity::sint Height,Unity::Texture2DPixelFormat::Type PixelFormat,Unity::sint StreamIndex);
__export bool			PopCast_UpdateTextureDebug(Unity::uint Instance,Unity::sint StreamIndex);
__export Unity::uint	PopCast_GetBackgroundGpuJobCount();
__export Unity::sint	PopCast_GetPendingFrameCount(Unity::uint Instance);

__export const char*	PopCast_GetMetaJson(Unity::uint Instance);
__export void			PopCast_ReleaseString(const char* String);

__export const char*	PopCast_PopDebugString();
__export void			PopCast_ReleaseDebugString(const char* String);
__export void			PopCast_ReleaseAllExports();



//	way too many problems with using a struct, reverting to bit flags. (which may also have problems with endianness, container size etc)
//		android ONLY struct is offset
//		IL2CPP bools are 32 bit and NOT garunteed to have bit 1 set if true
//		long is 32 bit on some platforms and 64 on others
namespace TPluginParams
{
	//	copy & paste this to c#
	enum PopCastFlags
	{
		None						= 0,
		ShowFinishedFile			= 1<<0,
		Gif_AllowIntraFrames		= 1<<1,
		Gif_DebugPalette			= 1<<2,
		Gif_DebugIndexes			= 1<<3,
		Gif_DebugTransparency		= 1<<4,
		SkipFrames					= 1<<5,
		Gif_CpuOnly					= 1<<6,
		Gif_LzwCompression			= 1<<7,
	};
}

namespace PopCast
{
	class TInstance;
	typedef Unity::uint	TInstanceRef;

	std::shared_ptr<TInstance>	Alloc(TCasterParams Params,std::shared_ptr<Opengl::TContext> OpenglContext=nullptr,std::shared_ptr<Directx::TContext> DirectxContext=nullptr);
	std::shared_ptr<TInstance>	GetInstance(TInstanceRef Instance);
	bool						Free(TInstanceRef Instance);
};

class TCastFrameMeta;


class PopCast::TInstance
{
public:
	TInstance()=delete;
	TInstance(const TInstance& Copy)=delete;
public:
	explicit TInstance(const TInstanceRef& Ref,TCasterParams& Params,std::shared_ptr<Opengl::TContext> OpenglContext,std::shared_ptr<Directx::TContext> DirectxContext);
	explicit TInstance(const TInstanceRef& Ref,TCasterParams& Params,std::shared_ptr<Opengl::TContext> OpenglContext) :
		TInstance	( Ref, Params, OpenglContext, nullptr )
	{
	}
	explicit TInstance(const TInstanceRef& Ref,TCasterParams& Params,std::shared_ptr<Directx::TContext> DirectxContext) :
		TInstance	( Ref, Params, nullptr, DirectxContext )
	{
	}
	
	TInstanceRef	GetRef() const		{	return mRef;	}
	
	void			WriteFrame(Opengl::TTexture& Texture,size_t StreamIndex);
	void			WriteFrame(Directx::TTexture& Texture,size_t StreamIndex);
	void			WriteFrame(std::shared_ptr<SoyPixelsImpl> Texture,size_t StreamIndex);
	
	void			GetMeta(TJsonWriter& Json);
	size_t			GetPendingPacketCount();

private:
	void			InitFrameMeta(TCastFrameMeta& Frame,size_t StreamIndex);
	
public:
	std::shared_ptr<Opengl::TContext>	mOpenglContext;
	std::shared_ptr<Directx::TContext>	mDirectxContext;
	std::shared_ptr<TCaster>			mCaster;
	SoyTime								mBaseTimestamp;

	std::shared_ptr<TPool<Directx::TTexture>>		mDirectxTexturePool;
	std::shared_ptr<TPool<Opengl::TTexture>>		mOpenglTexturePool;

private:
	TInstanceRef	mRef;
};


