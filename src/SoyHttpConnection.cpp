#include "SoyHttpConnection.h"





THttpConnection::THttpConnection(const std::string& Url) :
	SoyWorkerThread	( "THttpConnection", SoyWorkerWaitMode::Sleep ),
	mSocket			( new SoySocket )
{
	//	parse address
	mServerAddress = Url;
	std::string HttpPrefix("http://");
	if ( !Soy::StringTrimLeft( mServerAddress, HttpPrefix, false ) )
	{
		std::stringstream Error;
		Error << __func__ << " Url " << Url << " did not start with " << HttpPrefix;
		throw Soy::AssertException( Error.str() );
	}
	
	//	now split url from server
	BufferArray<std::string,2> ServerAndUrl;
	Soy::StringSplitByMatches( GetArrayBridge(ServerAndUrl), mServerAddress, "/" );
	Soy::Assert( ServerAndUrl.GetSize() != 0, "Url did not split at all" );
	if ( ServerAndUrl.GetSize() == 1 )
		ServerAndUrl.PushBack("/");
	
	mServerAddress = ServerAndUrl[0];
	
	//	if no port provided, try and add it
	{
		std::string Hostname;
		uint16 Port;
		try
		{
			Soy::SplitHostnameAndPort( Hostname, Port, mServerAddress );
		}
		catch (...)
		{
			mServerAddress += ":80";
		}
		
		//	fail if it doesn't succeed again
		Soy::SplitHostnameAndPort( Hostname, Port, mServerAddress );
	}
	
	std::Debug << "Split Http fetch to server=" << mServerAddress << std::endl;
}

void THttpConnection::Shutdown()
{
	if ( mSocket )
	{
		mSocket->Close();
	}
	
	if ( mReadThread )
	{
		mReadThread->WaitToFinish();
		mReadThread.reset();
	}

}


bool THttpConnection::Iteration()
{
	auto& Socket = *mSocket;
	try
	{
		Socket.CreateTcp(true);
		
		if ( !Socket.IsConnected() )
		{
			mConnectionRef = Socket.Connect( mServerAddress );
			
			//	if blocking and returns invalid, then the socket has probably error'd
			if ( !mConnectionRef.IsValid() )
			{
				throw Soy::AssertException("Connecton failed");
			}
			
			FlushRequestQueue();
			
			if ( !mReadThread )
			{
				auto OnResponse = [this](std::shared_ptr<Soy::TReadProtocol>& pProtocol)
				{
#if defined(ENABLE_RTTI)
					auto pHttpProtocol = std::dynamic_pointer_cast<Http::TResponseProtocol>( pProtocol );
#else
					auto pHttpProtocol = static_cast<Http::TResponseProtocol*>( pProtocol.get() );
#endif
					mOnResponse.OnTriggered( *pHttpProtocol );
				};
				
				mReadThread.reset( new THttpReadThread( mSocket, mConnectionRef ) );
				mOnDataRecievedListener = mReadThread->mOnDataRecieved.AddListener( OnResponse );
				mReadThread->Start();
			}
		}
	}
	catch ( std::exception& e )
	{
		Shutdown();
		std::stringstream Error;
		Error << "Failed to create & connect TCP socket: " << e.what();
		auto ErrorStr = Error.str();
		mOnError.OnTriggered( ErrorStr );
		return true;
	}
	
	//	only wake up when something happens now
	SetWakeMode( SoyWorkerWaitMode::Wake );
	return true;
}


void THttpConnection::SendRequest(std::shared_ptr<Http::TRequestProtocol> Request)
{
	Soy::Assert( Request != nullptr, "Request expected" );
	auto pRequest = std::dynamic_pointer_cast<Soy::TWriteProtocol>( Request );
	
	//	set host automatically
	if ( Request->mHost.empty() )
	{
		Request->mHost = mServerAddress;
	}
	
	mRequestQueue.PushBack( Request );
	FlushRequestQueue();
	
}

void THttpConnection::FlushRequestQueue()
{
	//	gotta wait for a connection!
	if ( !mSocket || !mConnectionRef.IsValid() )
		return;
	
	//	make thread if we don't have one
	if ( !mWriteThread )
	{
		mWriteThread.reset( new THttpWriteThread(mSocket,mConnectionRef) );
		mWriteThread->mOnStreamError.AddListener( mOnError );
		mWriteThread->Start();
		
	}
	
	while ( !mRequestQueue.IsEmpty() )
	{
		auto QueueRequest = mRequestQueue.PopAt(0);
		std::shared_ptr<Soy::TWriteProtocol> Request = QueueRequest;
		mWriteThread->Push( Request );
	}
}


void THttpConnection::SendRequest(const Http::TRequestProtocol& Request)
{
	std::shared_ptr<Http::TRequestProtocol> pRequest( new Http::TRequestProtocol(Request) );
	SendRequest( pRequest );
}

