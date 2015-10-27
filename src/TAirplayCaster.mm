#include "TAirplayCaster.h"


/*
//	http://www.engadget.com/2010/11/24/apple-tv-hacking-spelunking-into-the-airplay-video-service/#continued

//MPAirPlayVideoServiceBrowser and MPAirPlayVideoService

@class TAirplayBrowser;

@interface TAirplayBrowser : NSObject<MPAirPlayVideoServiceBrowser>
{
	GoogleCast::TContextInternal*	mParent;
}

- (id)initWithContext:(Airplay::TContextInternal*)parent;

-(void)_didFindService:(id)service moreComing:(BOOL)coming
{
	NSLog(@"Service Found. Attempting to resolve: %@ (%d more to come)", service, coming);
	[super _didFindService:service moreComing:coming];
	[[service retain] setDelegate:self];
	[service resolveWithTimeout:0.0f];
}


self.movieController = [[[MPMoviePlayerController alloc] initWithContentURL:[NSURL fileURLWithPath:CINDY_PATH]] autorelease];
[movieController setAllowsWirelessPlayback:YES];
movieController.view.frame = self.view.bounds;
[self.view addSubview:movieController.view];

@end


*/




class Airplay::TDeviceInternal
{
public:
	TDeviceInternal(TDevice& Parent) :
		mParent	( Parent )
	{
	}
	
	TDevice&					mParent;
};


class Airplay::TContextInternal
{
public:
	TContextInternal(TContext& Parent);
	~TContextInternal();
	
public:
	TContext&					mParent;
};



Airplay::TContextInternal::TContextInternal(TContext& Parent) :
	mParent		( Parent )
{
}

Airplay::TContextInternal::~TContextInternal()
{
}




Airplay::TContext::TContext() :
	mInternal		( new TContextInternal(*this) ),
	mMulticaster	( new SoyMulticaster("_airplay._tcp") )
{
	
}

Airplay::TContext::~TContext()
{
}


void Airplay::TContext::EnumDevices(ArrayBridge<TCastDeviceMeta>&& Metas)
{
	if ( !mInternal )
		return;
	
	Array<TServiceMeta> ServiceMetas;
	mMulticaster->EnumServices( GetArrayBridge(ServiceMetas) );
	
	//	convert
	for ( int m=0;	m<ServiceMetas.GetSize();	m++ )
	{
		auto& ServiceMeta = ServiceMetas[m];
		TCastDeviceMeta Meta;
		
		Meta.mVendor = ServiceMeta.mType;
		Meta.mName = ServiceMeta.mName;
		Meta.mSerial = ServiceMeta.mHostName;
		
		//	hostnames in multicast end in a . remove it
		std::string Hostname = ServiceMeta.mHostName;
		Soy::StringTrimRight( Hostname, ".", true );
		
		std::stringstream Address;
		Address << Hostname << ":" << ServiceMeta.mPort;
		Meta.mAddress = Address.str();
		
		Metas.PushBack( Meta );
	}
}


std::shared_ptr<Airplay::TDevice> Airplay::TContext::AllocDevice(TCasterParams Params)
{
	if ( !mInternal )
		return nullptr;
	
	Array<TCastDeviceMeta> Metas;
	EnumDevices( GetArrayBridge(Metas) );
	
	//	look for magic params
	if ( Params.mName == "*" )
	{
		if ( !Metas.IsEmpty() )
		{
			auto& Meta = Metas[0];
			return std::shared_ptr<Airplay::TDevice>( new Airplay::TMirrorDevice( Meta.mName, Meta.mAddress, Params ) );
		}
	}
	
	return nullptr;
}


Airplay::TMirrorDevice::TMirrorDevice(const std::string& Name,const std::string& Address,const TCasterParams& Params) :
	mInternal	( new TDeviceInternal(*this) ),
	mSentHeader	( false )
{
	//	http://nto.github.io/AirPlay.html#introduction
	auto OnConnected = [](bool&)
	{
		std::Debug << "AppleTv connection established" << std::endl;
	};
	auto OnHttpResponse = [](const Http::TResponseProtocol& Response)
	{
		std::Debug << "AppleTv got http response; " << Soy::ArrayToString( GetArrayBridge(Response.mContent) ) << std::endl;
	};
	auto OnError = [](const std::string& Error)
	{
		std::Debug << "AppleTv connection error: " << Error << std::endl;
	};

	//	mirroring has a specific port
	std::string Hostname;
	try
	{
		uint16 Port;
		Soy::SplitHostnameAndPort( Hostname, Port, Address );
	}
	catch(...)
	{
		Hostname = Address;
	}
	std::stringstream MirrorAddress;
	MirrorAddress << "http://" << Hostname << ":7100";
	
	//	create http connections to device
	mConnection.reset( new THttpConnection( MirrorAddress.str() ) );
	mConnection->mOnConnected.AddListener( OnConnected );
	mConnection->mOnResponse.AddListener( OnHttpResponse );
	mConnection->mOnError.AddListener( OnError );
	mConnection->Start();
	
	
	Http::TRequestProtocol ServerXmlRequest;
	ServerXmlRequest.mUrl = "stream.xml";
	ServerXmlRequest.mHeaders["Accept"] = "*/*";
	ServerXmlRequest.mHeaders["User-Agent"] = "curl/7.43.0";
	mConnection->SendRequest( ServerXmlRequest );
}


void Airplay::TMirrorDevice::Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context)
{
	throw Soy::AssertException("Not supported");
}

void Airplay::TMirrorDevice::Write(const std::shared_ptr<SoyPixelsImpl> Image,SoyTime Timecode)
{
	throw Soy::AssertException("Not supported");
}

