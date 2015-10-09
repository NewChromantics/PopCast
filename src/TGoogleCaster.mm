#include "TGoogleCaster.h"

#if defined(TARGET_OSX)
#include <OpenCast.h>
#include <OCMediaMetaData.h>
#include <OCDeviceScanner.h>
#elif defined(TARGET_IOS)
#include <GoogleCast/GoogleCast.h>
#endif

extern NSString *const kOCMetadataKeySubtitle;
#if defined(TARGET_OSX)
typedef OCDevice GCKDevice;
typedef OCDeviceScanner GCKDeviceScanner;
typedef OCDeviceManager GCKDeviceManager;
typedef OCMediaMetadata GCKMediaMetadata;
typedef OCImage GCKImage;
typedef OCMediaInformation GCKMediaInformation;
typedef OCMediaControlChannel GCKMediaControlChannel;
#define GCKDeviceScannerListener OCDeviceScannerListener 	//	typedef doesn't work on @protocol?
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



@class TScannerListener;

@interface TScannerListener : NSObject<GCKDeviceScannerListener>
{
	GoogleCast::TContextInternal*	mParent;
}

- (id)initWithContext:(GoogleCast::TContextInternal*)parent;
- (void)deviceDidComeOnline:(GCKDevice *)device;
- (void)deviceDidGoOffline:(GCKDevice *)device;

@end



class GoogleCast::TDeviceInternal
{
public:
	TDeviceInternal(TDevice& Parent) :
		mParent	( Parent )
	{
	}
	
	ObjcPtr<GCKDeviceManager>	mDeviceManager;
	ObjcPtr<GCKMediaControlChannel>	mMediaControl;
	TDevice&					mParent;
};

class GoogleCast::TContextInternal
{
public:
	TContextInternal(TContext& Parent);
	~TContextInternal();
	
	void						OnDeviceAdded(GCKDevice* Device);
	void						OnDeviceRemoved(GCKDevice* Device);
	void						EnumDevices(ArrayBridge<TCastDeviceMeta>& Metas);
	void						EnumDevices(ArrayBridge<TCastDeviceMeta>&& Metas)	{	EnumDevices( Metas );	}
	std::shared_ptr<ObjcPtr<GCKDevice>>	GetFirstDevice();
	std::shared_ptr<ObjcPtr<GCKDevice>>	GetDevice(const std::string& Name);
	
public:
	std::mutex					mDeviceLock;
	std::map<std::string,std::shared_ptr<ObjcPtr<GCKDevice>>>	mDevices;
	
	TContext&					mParent;
	ObjcPtr<GCKDeviceScanner>	mScanner;
	ObjcPtr<TScannerListener>	mScannerListener;
};





TCastDeviceMeta GetMeta(GCKDevice* Device)
{
	Soy::Assert( Device, "Device expected" );
	TCastDeviceMeta Meta;

	std::stringstream Address;
	
	auto RetainCount = [Device.ipAddress retainCount];
	std::Debug << "ip address retain count " << RetainCount << std::endl;
	
	Address << Soy::NSStringToString( Device.ipAddress ) << ":" << Device.servicePort;
	Meta.mAddress = Address.str();

	Meta.mSerial = Soy::NSStringToString(Device.deviceID);
	Meta.mName = Soy::NSStringToString(Device.friendlyName);
	Meta.mVendor = Soy::NSStringToString(Device.manufacturer);
	Meta.mModel = Soy::NSStringToString(Device.modelName);

	return Meta;
}



@implementation TScannerListener

- (id)initWithContext:(GoogleCast::TContextInternal*)parent
{
	self = [super init];
	if (self)
	{
		mParent = parent;
	}
	return self;
}

- (void)deviceDidComeOnline:(GCKDevice *)device
{
	mParent->OnDeviceAdded(device);
}

- (void)deviceDidGoOffline:(GCKDevice *)device
{
	mParent->OnDeviceRemoved(device);
}

@end



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
	
	mScannerListener.Retain( [[TScannerListener alloc] initWithContext:this] );

	[mScanner.mObject addListener:mScannerListener.mObject];
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


void GoogleCast::TContextInternal::OnDeviceAdded(GCKDevice* Device)
{
	std::lock_guard<std::mutex> Lock( mDeviceLock );

	std::shared_ptr<ObjcPtr<GCKDevice>> pDevice( new ObjcPtr<GCKDevice>() );
	pDevice->Retain( Device );
	
	auto Meta = GetMeta( Device );
	mDevices[Meta.mName] = pDevice;
}

void GoogleCast::TContextInternal::OnDeviceRemoved(GCKDevice* Device)
{
	std::lock_guard<std::mutex> Lock( mDeviceLock );
	
	auto Meta = GetMeta( Device );
	mDevices[Meta.mName].reset();
}

void GoogleCast::TContextInternal::EnumDevices(ArrayBridge<TCastDeviceMeta>& Metas)
{
	std::lock_guard<std::mutex> Lock( mDeviceLock );
	
	for ( auto it=mDevices.begin();	it!=mDevices.end();	it++ )
	{
		auto& DeviceName = it->first;
		auto& Device = it->second;
		TCastDeviceMeta Meta;
		
		if ( !Device )
		{
			Meta.mName = DeviceName;
		}
		else
		{
			Meta = GetMeta( Device->mObject );
		}
		Metas.PushBack( Meta );
	}
}

std::shared_ptr<ObjcPtr<GCKDevice>> GoogleCast::TContextInternal::GetDevice(const std::string& Name)
{
	std::lock_guard<std::mutex> Lock( mDeviceLock );
	auto Device = mDevices[Name];
	return Device;
}


std::shared_ptr<ObjcPtr<GCKDevice>> GoogleCast::TContextInternal::GetFirstDevice()
{
	std::lock_guard<std::mutex> Lock( mDeviceLock );

	for ( auto it=mDevices.begin();	it!=mDevices.end();	it++ )
	{
		auto& Device = it->second;
		if ( !Device )
			continue;
		return Device;
	}
	return nullptr;
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
	if ( !mInternal )
		return;
	
	mInternal->EnumDevices( Metas );
}


std::shared_ptr<GoogleCast::TDevice> GoogleCast::TContext::AllocDevice(TCasterParams Params)
{
	if ( !mInternal )
		return nullptr;
	
	std::shared_ptr<ObjcPtr<GCKDevice>> pDevice;
	if ( Params.mName == "*" )
	{
		pDevice = mInternal->GetFirstDevice();
	}
	else
	{
		pDevice = mInternal->GetDevice( Params.mName );
	}

	if ( !pDevice )
	{
		Array<TCastDeviceMeta> Metas;
		mInternal->EnumDevices( GetArrayBridge(Metas) );
		std::stringstream Error;
		Error << "No matching/alive chromecast name " << Params.mName << " out of " << Metas.GetSize() << " devices";
		throw Soy::AssertException( Error.str() );
	}
	
	std::shared_ptr<GoogleCast::TDevice> NewDevice( new GoogleCast::TDevice(pDevice->mObject) );
	return NewDevice;
}


GoogleCast::TDevice::TDevice(void* _GCKDevice) :
	mInternal	( new TDeviceInternal(*this) )
{
	auto& mDeviceManager = mInternal->mDeviceManager;
	GCKDevice* Device = reinterpret_cast<GCKDevice*>(_GCKDevice);
	
	//	create new device interface and connect to it
	NSString* PackageName = @"CBFF3EA9";
	mDeviceManager.Retain( [[GCKDeviceManager alloc]initWithDevice:Device clientPackageName:PackageName] );
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
	[Meta addImage:[[GCKImage alloc]initWithURL:[[NSURL alloc] initWithString:ImageUrlNs] width:Media.mImageWidth height:Media.mImageHeight]];
#endif
	
	GCKMediaInformation *mediaInformation =
	[[GCKMediaInformation alloc] initWithContentID:MediaUrlNs
										streamType:GCKMediaStreamTypeNone
									   contentType:ContentTypeNs
										  metadata:Meta
									streamDuration:Media.mDuration
										customData:nil];
	
	
	auto Result = [mMediaControl loadMedia:mediaInformation autoplay:Media.mAutoPlay?YES:NO playPosition:Media.mStartTime];
	std::Debug << "Load media result: " << Result << std::endl;
}

void GoogleCast::TDevice::Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context)
{
	throw Soy::AssertException("Not supported");
}

void GoogleCast::TDevice::Write(const std::shared_ptr<SoyPixelsImpl> Image,SoyTime Timecode)
{
	throw Soy::AssertException("Not supported");
}

