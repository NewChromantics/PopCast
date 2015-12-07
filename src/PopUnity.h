#pragma once

#include <SoyUnity.h>


namespace Unity
{
	typedef void (*OpenglJobCallback)();

	class TGlobalParams;
	
	extern TGlobalParams	gParams;
};




//	gr: when using multiple plugins on IOS, this function can get linked wrong and could never get called. So have a seperate function name which should always resolve.
//		compiler DOES NOT issue a link error like it should
//		this would apply to any statically linked lib
#if defined(TARGET_IOS)
__export void UnityRenderEvent_Ios_PopCastTexture(Unity::sint eventID);
#else
__export void UnityRenderEvent(Unity::sint eventID);
#endif

//	unique eventid so that other plugins don't trigger redundant opengl thread calls
__export Unity::sint PopCast_GetPluginEventId();



class Unity::TGlobalParams
{
public:
	Unity::sint		mEventId = 0x12345678;
	bool			mDebugPluginEvent = false;
	size_t			mMinTimerMs = 5;
};

