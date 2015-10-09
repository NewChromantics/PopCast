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
	mInternal	( new TContextInternal(*this) )
{
}

Airplay::TContext::~TContext()
{
}


void Airplay::TContext::EnumDevices(ArrayBridge<TCastDeviceMeta>&& Metas)
{
	if ( !mInternal )
		return;
	
	//mInternal->EnumDevices( Metas );
}


std::shared_ptr<Airplay::TDevice> Airplay::TContext::AllocDevice(TCasterParams Params)
{
	if ( !mInternal )
		return nullptr;
	
	return nullptr;
}


Airplay::TDevice::TDevice(const TCasterParams& Params) :
	mInternal	( new TDeviceInternal(*this) )
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
	
	//	create http connections to device
	mConnection.reset( new THttpConnection( Params.mName ) );
	mConnection->mOnConnected.AddListener( OnConnected );
	mConnection->mOnResponse.AddListener( OnHttpResponse );
	mConnection->mOnError.AddListener( OnError );
	mConnection->Start();
}


void Airplay::TDevice::Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context)
{
	throw Soy::AssertException("Not supported");
}

void Airplay::TDevice::Write(const std::shared_ptr<SoyPixelsImpl> Image,SoyTime Timecode)
{
	throw Soy::AssertException("Not supported");
}

