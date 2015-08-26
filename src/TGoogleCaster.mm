#include "TGoogleCaster.h"
#include <GoogleCast/GoogleCast.h>



void GoogleCast::EnumDevices(ArrayBridge<TCastDeviceMeta>&& Metas)
{
	TCastDeviceMeta Test;
	Test.mName = "Test";
	Metas.PushBack( Test );
}


