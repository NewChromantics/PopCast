#pragma once

#include <SoySocket.h>
#include <SoyStream.h>
#include <SoyHttp.h>
#include <SoyProtocol.h>



class TSocketReadThread : public TStreamReader
{
public:
	TSocketReadThread(std::shared_ptr<SoySocket>& Socket,SoyRef ConnectionRef);
	
	virtual void					Read() override;	//	read next chunk of data into buffer
	
public:
	Array<char>						mRecvBuffer;		//	static buffer, just alloc once
	SoyRef							mConnectionRef;
	std::shared_ptr<SoySocket>		mSocket;			//	socket we're reading from
};


class TSocketWriteThread : public TStreamWriter
{
public:
	TSocketWriteThread(std::shared_ptr<SoySocket>& Socket,SoyRef ConnectionRef);
	
protected:
	virtual void					Write(TStreamBuffer& Buffer) override;
	
private:
	Array<char>						mSendBuffer;		//	static buffer, just save realloc
	SoyRef							mConnectionRef;
	std::shared_ptr<SoySocket>		mSocket;			//	socket we're writing to
};



class THttpReadThread : public TSocketReadThread
{
public:
	THttpReadThread(std::shared_ptr<SoySocket>& Socket,SoyRef ConnectionRef) :
	TSocketReadThread	( Socket, ConnectionRef )
	{
	}
	
	virtual std::shared_ptr<Soy::TReadProtocol>	AllocProtocol() override
	{
		return std::shared_ptr<Soy::TReadProtocol>( new Http::TResponseProtocol );
	}
};

class THttpWriteThread : public TSocketWriteThread
{
public:
	THttpWriteThread(std::shared_ptr<SoySocket>& Socket,SoyRef ConnectionRef) :
	TSocketWriteThread	( Socket, ConnectionRef )
	{
	}
	
};


class THttpConnection : public SoyWorkerThread
{
public:
	THttpConnection(const std::string& Url);
	~THttpConnection()		{	Shutdown();	}
	
	void			SendRequest(const Http::TRequestProtocol& Request);
	void			SendRequest(std::shared_ptr<Http::TRequestProtocol> Request);

private:
	virtual bool	Iteration() override;
	void			Shutdown();
	
public:
	std::string						mServerAddress;
	std::string						mUrl;
	
	SoyRef							mConnectionRef;
	std::shared_ptr<SoySocket>		mSocket;
	SoyEvent<const std::string>		mOnError;
	SoyEvent<bool>					mOnConnected;
	SoyEvent<const Http::TResponseProtocol>	mOnResponse;
	
	std::shared_ptr<THttpReadThread>	mReadThread;
	std::shared_ptr<THttpWriteThread>	mWriteThread;
	SoyListenerId						mOnDataRecievedListener;
};




