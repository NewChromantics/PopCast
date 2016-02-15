#pragma once

#include <SoyStream.h>
#include <SoyHttpServer.h>


//	server
//		files
//			single input
//			multiple clients
class THttpPortServer;
class THttpFileServer;
class THttpFileClient;

//	bridges
class THttpFileWriter;


//	bridge between server & file
class THttpFileWriter : public TStreamWriter
{
public:
	THttpFileWriter(const std::string& PortAndPath);
	
	virtual void		Write(TStreamBuffer& Buffer) override;
	
public:
	std::shared_ptr<THttpFileServer>	mFileServer;
};



//	bridge between client connection  & file
class THttpFileClient
{
public:
	THttpFileClient(THttpServer& Server,SoyRef Connection);
	
	void					PushData(ArrayBridge<uint8>&& Data);

public:
	TStreamBuffer								mBuffer;
	std::shared_ptr<Http::TResponseProtocol>	mInfiniteResponse;
};




//	"a file" with all its connections
class THttpFileServer
{
public:
	THttpFileServer();
	
	void								Write(TStreamBuffer& Buffer);
	std::shared_ptr<THttpFileClient>	StartResponse(THttpServer& Server,SoyRef Connection);
	
public:
	//	output
	std::mutex								mConnectionsLock;
	std::map<SoyRef,std::shared_ptr<THttpFileClient>>	mConnections;
	
	//	need to do this formally...
	std::mutex		mDataLock;
	Array<uint8>	mDataHeader;
};


	
	
//	port handler, redirects requests for particular files
class THttpPortServer
{
public:
	THttpPortServer(size_t Port);
	
	std::shared_ptr<THttpFileServer>	AllocFileServer(const std::string& Path);
	std::shared_ptr<THttpFileServer>	GetFileServer(const std::string& Path);
	std::shared_ptr<THttpFileClient>	AllocFileClient(const std::string& Path,SoyRef ClientRef);
	
	void								SendFileNotFound(SoyRef ClientRef,const Http::TRequestProtocol& Request);
	
public:
	std::shared_ptr<THttpServer>							mServer;
	std::mutex												mFileServersLock;
	std::map<std::string,std::shared_ptr<THttpFileServer>>	mFileServers;
};

