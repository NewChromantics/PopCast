#pragma once

#include <SoySocket.h>
#include <SoyStream.h>
#include <SoyHttp.h>
#include <SoyProtocol.h>
#include <SoySocketStream.h>



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
	void			FlushRequestQueue();		//	we queue requests until we're connected
	
public:
	std::string						mServerAddress;
	
	SoyRef							mConnectionRef;
	std::shared_ptr<SoySocket>		mSocket;
	SoyEvent<const std::string>		mOnError;
	SoyEvent<bool>					mOnConnected;
	SoyEvent<const Http::TResponseProtocol>	mOnResponse;
	
	std::shared_ptr<THttpReadThread>	mReadThread;
	std::shared_ptr<THttpWriteThread>	mWriteThread;
	SoyListenerId						mOnDataRecievedListener;
	
	Array<std::shared_ptr<Http::TRequestProtocol>>	mRequestQueue;
	
};




