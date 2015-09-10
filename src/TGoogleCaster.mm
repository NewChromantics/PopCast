#include "TGoogleCaster.h"

#if defined(TARGET_OSX)
#include <OpenCast.h>
#elif defined(TARGET_IOS)
#include <GoogleCast/GoogleCast.h>
#endif


#if defined(TARGET_OSX)
typedef OCDevice GCKDevice;
typedef OCDeviceScanner GCKDeviceScanner;
#endif




class GoogleCast::TContextInternal
{
public:
	TContextInternal(TContext& Parent);
	~TContextInternal();
	
public:
	TContext&					mParent;
	ObjcPtr<OCDeviceScanner>	mScanner;
};




GoogleCast::TContextInternal::TContextInternal(TContext& Parent) :
	mParent		( Parent )
{
	auto AppNameNs = Soy::StringToNSString( mParent.mAppName );
	
#if defined(TARGET_OSX)
	AppNameNs = kOCMediaDefaultReceiverApplicationID;
#elif defined(TARGET_IOS)
	AppNameNs = kGCKMediaDefaultReceiverApplicationID;
#endif
	
#if defined(TARGET_OSX)
	mScanner.Retain( [[OCDeviceScanner alloc] init] );
#elif defined(TARGET_IOS)
	GCKFilterCriteria* Filter = [GCKFilterCriteria criteriaForAvailableApplicationWithID:AppNameNs];
	mScanner.Retain( [[GCKDeviceScanner alloc] initWithFilterCriteria:Filter] );
#endif

	[mScanner.mObject startScan];
}

GoogleCast::TContextInternal::~TContextInternal()
{
	if ( mScanner )
	{
		[mScanner.mObject stopScan];
		mScanner.Release();
	}
}




GoogleCast::TContext::TContext(const std::string& AppName) :
	mAppName	( AppName )
{
	/*
	 gr: staticcally linking atm as symbols aren't resolving afterwards atm
	//	make sure the framework is loaded by making sure a symbol exists
	auto LoadLibraryTest = []
	{
//		auto* Addr = &kOCMediaDefaultReceiverApplicationID;
		auto Class = [OCDeviceScanner class];
		return Class != nullptr;
	};
	
	//	load framework
	mLibrary.reset( new Soy::Platform::TLibrary("Assets/Plugins/Osx/PopCastOsx.bundle/Contents/Resources/OpenCast.framework/OpenCast", LoadLibraryTest ) );
//	mLibrary.reset( new Soy::Platform::TLibrary("Assets/Plugins/Osx/PopCastOsx.bundle/Contents/Resources/OpenCast.framework/OpenCast", nullptr ) );
	

//	mLibrary->ReResolve( &kOCMediaDefaultReceiverApplicationID, "kOCMediaDefaultReceiverApplicationID" );
	*/
	mInternal.reset( new TContextInternal(*this) );
}

GoogleCast::TContext::~TContext()
{
}


void GoogleCast::TContext::EnumDevices(ArrayBridge<TCastDeviceMeta>&& Metas)
{
	std::lock_guard<std::mutex> Lock( mDevicesLock );

	mDevices.Clear();
	
	//	grab list
	auto Devices = mInternal->mScanner.mObject.devices;
	
	for ( GCKDevice* Device in Devices )
	{
		TCastDeviceMeta Meta;
		std::stringstream Address;
		Address << Soy::NSStringToString( Device.ipAddress ) << ":" << Device.servicePort;
		Meta.mAddress = Address.str();
		
		Meta.mSerial = Soy::NSStringToString(Device.deviceID);
		Meta.mName = Soy::NSStringToString(Device.friendlyName);
		Meta.mVendor = Soy::NSStringToString(Device.manufacturer);
		Meta.mModel = Soy::NSStringToString(Device.modelName);
	
		mDevices.PushBack( Meta );
	}
	
	
	Metas.PushBackArray( mDevices );
}

