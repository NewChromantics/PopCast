#include "PopUnity.h"

namespace Unity
{
	TGlobalParams	gParams;
}


#if defined(TARGET_IOS)
__export void UnityRenderEvent_Ios_PopCast(Unity::sint eventID)
#else
__export void UnityRenderEvent(Unity::sint eventID)
#endif
{
	//	event triggered by other plugin
	if ( eventID != PopCast_GetPluginEventId() )
	{
		if ( Unity::gParams.mDebugPluginEvent )
			std::Debug << "UnityRenderEvent(" << eventID << ") Not PopMovieEvent " << Unity::gParams.mEventId << std::endl;
		return;
	}

	Unity::RenderEvent( eventID );
}

__export Unity::sint PopCast_GetPluginEventId()
{
	return Unity::gParams.mEventId;
}



