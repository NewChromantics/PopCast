#include "SoyHttpConnection.h"





TSocketReadThread::TSocketReadThread(std::shared_ptr<SoySocket>& Socket,SoyRef ConnectionRef) :
TStreamReader	( "TSocketReadThread" ),
mSocket			( Socket ),
mConnectionRef	( ConnectionRef )
{
	
}



void TSocketReadThread::Read()
{
	SoySocketConnection	ClientSocket = mSocket->GetConnection( mConnectionRef );
	if ( !ClientSocket.IsValid() )
	{
		//	lost connection
		throw Soy::AssertException("Connection lost");
	}
	
	//	gr use a larger static buffer so we can stream locally much faster than in 1024 chunks
	//static int BufferSize = 1024*1024*20;	//	Xmb a time should be plenty... maybe query for the actual socket limit jsut so we don't go silly with memory
	static int BufferSize = 100;	//	Xmb a time should be plenty... maybe query for the actual socket limit jsut so we don't go silly with memory
	auto& Buffer = mRecvBuffer;
	Buffer.SetSize( BufferSize );
	
	
	//	throws on error
	if ( !ClientSocket.Recieve( GetArrayBridge(Buffer) ) )
	{
		//	graceful close
		throw Soy::AssertException("Gracefull close");
	}
	
	mReadBuffer.Push( GetArrayBridge( Buffer ) );
}



TSocketWriteThread::TSocketWriteThread(std::shared_ptr<SoySocket>& Socket,SoyRef ConnectionRef) :
TStreamWriter	( "TSocketWriteThread" ),
mSocket			( Socket ),
mConnectionRef	( ConnectionRef )
{
}

void TSocketWriteThread::Write(TStreamBuffer& Buffer)
{
	//	keep writing until buffer is empty
	while ( !Buffer.IsEmpty() )
	{
		SoySocketConnection	ClientSocket = mSocket->GetConnection( mConnectionRef );
		if ( !ClientSocket.IsValid() )
		{
			//	lost connection
			throw Soy::AssertException("Connection lost");
		}
		
		static size_t PopMaxSize = 1024 * 1024 * 1;
		size_t PopSize = std::min( PopMaxSize, Buffer.GetBufferedSize() );
		if ( !Buffer.Pop( PopSize, GetArrayBridge(mSendBuffer) ) )
		{
			std::Debug << "Buffer data has gone missing. Trying again" << std::endl;
			continue;
		}
		
		//	write to socket
		try
		{
			ClientSocket.Send( GetArrayBridge(mSendBuffer), mSocket->IsUdp() );
		}
		catch (std::exception& e)
		{
			mSocket->OnError( mConnectionRef, e.what() );
			throw;
		}
	}
}






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
		if ( !SoySocket::GetHostnameAndPortFromAddress( Hostname, Port, mServerAddress ) )
		{
			mServerAddress += ":80";
		}
		
		//	fail if it doesn't succeed again
		if ( !SoySocket::GetHostnameAndPortFromAddress( Hostname, Port, mServerAddress ) )
		{
			std::stringstream Error;
			Error << "Failed to split hostname & port from " << ServerAndUrl[0];
			throw Soy::AssertException( Error.str() );
		}
	}
	
	
	mUrl = ServerAndUrl[1];
	
	std::Debug << "Split Http fetch to server=" << mServerAddress << " and url=" << mUrl << std::endl;
}

void THttpConnection::Shutdown()
{
	if ( mReadThread )
	{
		mReadThread->WaitToFinish();
		mReadThread.reset();
	}
	
	if ( mSocket )
	{
		mSocket->Close();
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
	
	//	make thread if we don't have one
	if ( !mWriteThread )
	{
		mWriteThread.reset( new THttpWriteThread(mSocket,mConnectionRef) );
		mWriteThread->Start();
	}
	
	mWriteThread->Push( pRequest );
}

void THttpConnection::SendRequest(const Http::TRequestProtocol& Request)
{
	std::shared_ptr<Http::TRequestProtocol> pRequest( new Http::TRequestProtocol(Request) );
	SendRequest( pRequest );
}

