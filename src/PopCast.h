#pragma once

#include "SoyDebug.h"
#include "PopUnity.h"


class TCaster;
class TCasterParams;

__export Unity::ulong	PopCast_Alloc(const char* Filename);
__export bool			PopCast_Free(Unity::ulong Instance);
__export void			PopCast_EnumDevices();
__export bool			PopCast_UpdateRenderTexture(Unity::ulong Instance,Unity::NativeTexturePtr TextureId,Unity::sint Width,Unity::sint Height,Unity::RenderTexturePixelFormat::Type PixelFormat);
__export bool			PopCast_UpdateTexture2D(Unity::ulong Instance,Unity::NativeTexturePtr TextureId,Unity::sint Width,Unity::sint Height,Unity::Texture2DPixelFormat::Type PixelFormat);



namespace PopCast
{
	class TInstance;
	typedef Unity::ulong	TInstanceRef;

	std::shared_ptr<TInstance>	Alloc(TCasterParams Params);
	std::shared_ptr<TInstance>	GetInstance(TInstanceRef Instance);
	bool						Free(TInstanceRef Instance);
};



class PopCast::TInstance
{
public:
	TInstance()=delete;
	TInstance(const TInstance& Copy)=delete;
public:
	explicit TInstance(const TInstanceRef& Ref,TCasterParams Params);
	
	TInstanceRef	GetRef() const		{	return mRef;	}
	
	void			WriteFrame(Opengl::TTexture Texture,Opengl::TContext& Context);
	
public:
	std::shared_ptr<TCaster>	mCaster;
	SoyTime						mBaseTimestamp;

private:
	TInstanceRef	mRef;
};


