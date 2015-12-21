#include "PopUnity.h"




__export Unity::sint PopCast_GetPluginEventId()
{
	return Unity::GetPluginEventId();
}

int Unity::GetPluginEventId()
{
	return mEventId;
}


