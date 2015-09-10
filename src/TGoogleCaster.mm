#include "TGoogleCaster.h"

#if defined(TARGET_OSX)
#include <OpenCast.h>
#include <OCMediaMetaData.h>
#elif defined(TARGET_IOS)
#include <GoogleCast/GoogleCast.h>
#endif

extern NSString *const kOCMetadataKeySubtitle;
#if defined(TARGET_OSX)
typedef OCDevice GCKDevice;
typedef OCDeviceScanner GCKDeviceScanner;
typedef OCMediaMetadata GCKMediaMetadata;
typedef OCImage GCKImage;
typedef OCMediaInformation GCKMediaInformation;
typedef OCMediaControlChannel GCKMediaControlChannel;
auto kGCKMetadataKeyTitle = kOCMetadataKeyTitle;
auto kGCKMetadataKeySubtitle = kOCMetadataKeySubtitle;
auto GCKMediaStreamTypeNone = OCMediaStreamTypeNone;
auto GCKMediaStreamTypeBuffered = OCMediaStreamTypeBuffered;
auto GCKMediaStreamTypeLive = OCMediaStreamTypeLive;
auto GCKMediaStreamTypeUnknown = OCMediaStreamTypeUnknown;
#endif


//	gr: these are currently missing from opencast
#if defined(TARGET_OSX)
NSString* const kOCMetadataKeyTitle = @"Title";
NSString* const kOCMetadataKeySubtitle = @"SubTitle";
#endif

class GoogleCast::TDeviceInternal
{
public:
	TDeviceInternal(TDevice& Parent) :
		mParent	( Parent )
	{
	}
	
	ObjcPtr<OCDeviceManager>	mDeviceManager;
	ObjcPtr<GCKMediaControlChannel>	mMediaControl;
	TDevice&					mParent;
};

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
		ObjcPtr<GCKDevice> _Device( Device );
		TCastDeviceMeta Meta;
		std::stringstream Address;
		//	gr: this has suddenly started crashing?
		//Address << Soy::NSStringToString( Device.ipAddress ) << ":" << Device.servicePort;
		Meta.mAddress = Address.str();
		
		Meta.mSerial = Soy::NSStringToString(Device.deviceID);
		Meta.mName = Soy::NSStringToString(Device.friendlyName);
		Meta.mVendor = Soy::NSStringToString(Device.manufacturer);
		Meta.mModel = Soy::NSStringToString(Device.modelName);
		Meta.mReference = Device;
	
		mDevices.PushBack( Meta );
	}
	
	
	Metas.PushBackArray( mDevices );
}


std::shared_ptr<GoogleCast::TDevice> GoogleCast::TContext::AllocDevice(TCasterParams Params)
{
	//	find device
	Array<TCastDeviceMeta> Metas;
	EnumDevices( GetArrayBridge(Metas) );

	//	find match
	GCKDevice* Device = nullptr;
	for ( int m=0;	m<Metas.GetSize();	m++ )
	{
		auto& Meta = Metas[m];
		if ( Params.mName == "*" )
		{
		}
		else if ( !Soy::StringBeginsWith( Meta.mName, Params.mName, false ) )
		{
			continue;
		}
		Device = reinterpret_cast<GCKDevice*>(Meta.mReference);
		break;
	}

	if ( !Device )
	{
		std::stringstream Error;
		Error << "No matching chromecast name " << Params.mName << " out of " << Metas.GetSize() << " devices";
		throw Soy::AssertException( Error.str() );
	}
	
	std::shared_ptr<GoogleCast::TDevice> NewDevice( new GoogleCast::TDevice(Device) );
	return NewDevice;
}


GoogleCast::TDevice::TDevice(void* _GCKDevice) :
	mInternal	( new TDeviceInternal(*this) )
{
	auto& mDeviceManager = mInternal->mDeviceManager;
	GCKDevice* Device = reinterpret_cast<GCKDevice*>(_GCKDevice);
	
	//	create new device interface and connect to it
	NSString* PackageName = @"CBFF3EA9";
	mDeviceManager.Retain( [[OCDeviceManager alloc]initWithDevice:Device clientPackageName:PackageName] );
	if ( !mDeviceManager )
		throw Soy::AssertException("Failed to allocate device manager");
	
	Connect();
}

void GoogleCast::TDevice::Connect()
{
	auto& mDeviceManager = mInternal->mDeviceManager;
	[mDeviceManager.mObject connect];

	TMediaMeta Media;
	Media.mTitle = "Title";
	Media.mSubtitle = "Subtitle";
	Media.mImageUrl = "http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/images/BigBuckBunny.jpg";
	Media.mImageWidth = 480;
	Media.mImageHeight = 360;
	Media.mMediaUrl = "http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4";
	Media.mDuration = 0;
	Media.mContentType = "video/mp4";
	Media.mStartTime = 0;
	Media.mAutoPlay = true;

	LoadMedia( Media );
}



void GoogleCast::TDevice::LoadMedia(const TMediaMeta& Media)
{
	//	create media controller
	auto& mMediaControl = mInternal->mMediaControl;
	auto& DeviceManager = mInternal->mDeviceManager.mObject;
	
	/*
	- (void)deviceManager:(GCKDeviceManager *)deviceManager
didConnectToCastApplication:(GCKApplicationMetadata *)applicationMetadata
sessionID:(NSString *)sessionID
launchedApplication:(BOOL)launchedApplication {
	*/
	mMediaControl.Retain( [[GCKMediaControlChannel alloc] init] );
//	self.mediaControlChannel.delegate = self;
	[DeviceManager addChannel:mMediaControl.mObject];
//	[mMediaControl requestStatus];
	
	
	auto TitleNs = Soy::StringToNSString( Media.mTitle );
	auto SubtitleNs = Soy::StringToNSString( Media.mSubtitle );
	auto ImageUrlNs = Soy::StringToNSString( Media.mImageUrl );
	auto MediaUrlNs = Soy::StringToNSString( Media.mMediaUrl );
	auto ContentTypeNs = Soy::StringToNSString( Media.mContentType );
	
	GCKMediaMetadata* Meta = [[GCKMediaMetadata alloc] init];
	[Meta setString:TitleNs forKey:kGCKMetadataKeyTitle];
	[Meta setString:SubtitleNs forKey:kGCKMetadataKeySubtitle];
#if !defined(TARGET_OSX)
	[Meta addImage:[[GCKImage alloc]initWithURL:[[NSURL alloc] initWithString:ImageUrlNs] width:ImageWidth height:ImageHeight]];
#endif
	
	GCKMediaInformation *mediaInformation =
	[[GCKMediaInformation alloc] initWithContentID:MediaUrlNs
										streamType:GCKMediaStreamTypeNone
									   contentType:ContentTypeNs
										  metadata:Meta
									streamDuration:Media.mDuration
										customData:nil];
	
	
	[mMediaControl loadMedia:mediaInformation autoplay:Media.mAutoPlay?YES:NO playPosition:Media.mStartTime];
}

