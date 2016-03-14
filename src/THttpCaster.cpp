#include "THttpCaster.h"
#include "SoyHttpServer.h"




namespace PopCast
{
	namespace Private
	{
		std::mutex											HttpServersLock;
		std::map<size_t,std::shared_ptr<THttpPortServer>>	HttpServers;
	}
	
	std::shared_ptr<THttpPortServer>	GetPortServer(size_t Port);
	
	
	const char*		DefaultHttpPath = "/";
	
	//	only root can have a port < 1024
#if defined(TARGET_OSX)
	size_t			DefaultHttpPort = 8080;
#else
	size_t			DefaultHttpPort = 80;
#endif
}


	
void SplitPortAndPath(std::string PortAndPath,size_t& Port,std::string& Path)
{
	//	eat prefix
	Soy::StringTrimLeft( PortAndPath, "http:", false );
	
	//	allow these;
	//	80			(80 & /)
	//	80/
	//	80/cat.gif
	//	/cat.gif	(80 & /cat.gif)
	//	cat.gif		(80 & /cat.gif)
	//	""			(80 & /)
	//	cat/dog.gif	(80 & /cat/dog.gif)
	
	if ( PortAndPath.empty() )
	{
		Port = PopCast::DefaultHttpPort;
		Path = PopCast::DefaultHttpPath;
		return;
	}
	
	//	look for the first slash
	auto PathPos = PortAndPath.find('/');

	//	no slash, so either port or path
	if ( PathPos == std::string::npos )
	{
		//	pure integer = port
		if ( Soy::StringToUnsignedInteger( Port, PortAndPath ) )
		{
			Path = PopCast::DefaultHttpPath;
		}
		else
		{
			Path = "/";
			Path += PortAndPath;
			Port = PopCast::DefaultHttpPort;
		}
	}
	else if ( PathPos == 0 )
	{
		Port = PopCast::DefaultHttpPort;
		Path = PortAndPath;
	}
	else
	{
		//	if the first part of the path is not integer... it's all a path!
		std::string PartOne = PortAndPath.substr( 0, PathPos );
		std::string PartTwo = PortAndPath.substr( PathPos );
		if ( Soy::StringToUnsignedInteger( Port, PartOne ) )
		{
			Path = PartTwo;
		}
		else
		{
			Port = PopCast::DefaultHttpPort;
			Path = "/";
			Path += PortAndPath;
		}
	}
}


std::shared_ptr<THttpPortServer> PopCast::GetPortServer(size_t Port)
{
	std::lock_guard<std::mutex> Lock( Private::HttpServersLock );
	
	auto it = Private::HttpServers.find( Port );
	if ( it == Private::HttpServers.end() )
	{
		//	throw before altering map
		auto pServer = std::make_shared<THttpPortServer>(Port);
		Private::HttpServers[Port] = pServer;
	}

	return Private::HttpServers[Port];
}


THttpFileWriter::THttpFileWriter(const std::string& PortAndPath) :
	TStreamWriter	( std::string("THttpFileWriter " + PortAndPath ) )
{
	size_t Port;
	std::string Path;
	SplitPortAndPath( PortAndPath, Port, Path );
	auto pServer = PopCast::GetPortServer( Port );
	
	mFileServer = pServer->AllocFileServer( Path );
}

void THttpFileWriter::Write(TStreamBuffer& Buffer,const std::function<bool()>& Block)
{
	Soy::Assert( mFileServer != nullptr, "Missing Http file server");
	mFileServer->Write( Buffer );
}



THttpFileServer::THttpFileServer()
{
	
}

void THttpFileServer::Write(TStreamBuffer& Buffer)
{
	auto Length = Buffer.GetBufferedSize();
	
	Array<uint8> NewData;
	Buffer.Pop( Length, GetArrayBridge( NewData ) );
	
	{
		std::lock_guard<std::mutex> Lock( mConnectionsLock );
		for ( auto& it : this->mConnections )
		{
			auto pConnection = it.second;
			
			//	gr: null??? how has this happened
			if ( pConnection == nullptr )
				continue;
			
			pConnection->PushData( GetArrayBridge(NewData) );
		}
	}
	
	std::lock_guard<std::mutex> Lock( mDataLock );
	mDataHeader.PushBackArray( NewData );
}

std::shared_ptr<THttpFileClient> THttpFileServer::StartResponse(THttpServer& Server,SoyRef ConnectionRef)
{
	std::lock_guard<std::mutex> Lock( mConnectionsLock );
	auto& Connection = mConnections[ConnectionRef];
	
	if ( Connection )
	{
		std::Debug << "Warning: FileServer client connection " << ConnectionRef << " already exists" << std::endl;
	}

	if ( !Connection )
	{
		Connection.reset( new THttpFileClient( Server, ConnectionRef ) );
		
		//	send header data
		std::lock_guard<std::mutex> DataLock( mDataLock );
		Connection->PushData( GetArrayBridge(mDataHeader) );
	}

	return Connection;
}


THttpFileClient::THttpFileClient(THttpServer& Server,SoyRef Connection)
{
	mInfiniteResponse.reset( new Http::TResponseProtocol( mBuffer ) );
	auto& Response = *mInfiniteResponse;
	
	Response.SetContentType( SoyMediaFormat::Gif );
	
	Server.SendResponse( mInfiniteResponse, Connection );
}


void THttpFileClient::PushData(ArrayBridge<uint8>&& Data)
{
	mBuffer.Push( Data );
}


THttpPortServer::THttpPortServer(size_t Port)
{
	auto OnRequest = [this](const Http::TRequestProtocol& Request,SoyRef Client)
	{
		auto FileServer = GetFileServer( Request.mUrl );
		if ( !FileServer )
		{
			SendFileNotFound( Client, Request );
		}
		else
		{
			FileServer->StartResponse( *mServer, Client );
		}
	};
	
	mServer.reset( new THttpServer( Port, OnRequest ) );
}

						 
std::shared_ptr<THttpFileServer> THttpPortServer::AllocFileServer(const std::string& Path)
{
	std::lock_guard<std::mutex> Lock( mFileServersLock );
	auto it = mFileServers.find( Path );
	if ( it == mFileServers.end() )
	{
		auto FileServer = std::make_shared<THttpFileServer>();
		mFileServers[Path] = FileServer;
	}
	return mFileServers[Path];
}


std::shared_ptr<THttpFileServer> THttpPortServer::GetFileServer(const std::string& Path)
{
	std::lock_guard<std::mutex> Lock( mFileServersLock );
	auto it = mFileServers.find( Path );
	if ( it == mFileServers.end() )
		return nullptr;

	return mFileServers[Path];
}

void THttpPortServer::SendFileNotFound(SoyRef Client,const Http::TRequestProtocol& Request)
{
	Http::TResponseProtocol Response;
	
	//Response.mResponseCode = 404;
	//Response.mResponseUrl = "not found";
	Response.mKeepAlive = false;

	std::stringstream Content;
	Content << "<h1>404</h1>";
	Content << "<p>File not found: " << Request.mUrl << "</p>";
	Content << "<ul>";
	for ( auto& f : mFileServers )
	{
		auto Filename = f.first;
		Content << "<li><a href=\"" << Filename << "\">" << Filename << "</a></li>";
	}
	Content << "</ul>";
	Response.SetContent( Content.str(), SoyMediaFormat::Html );
	
	mServer->SendResponse( Response, Client );
}








