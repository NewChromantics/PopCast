#pragma once

#include <SoyMedia.h>
#include "libavwrapper/LibavWrapper.h"


namespace Libav
{
	class TMuxer;
};



class Libav::TMuxer : public TMediaMuxer
{
public:
	TMuxer(std::shared_ptr<TStreamWriter>& Output,std::shared_ptr<TMediaPacketBuffer>& Input);
	
protected:
	virtual void	SetupStreams(const ArrayBridge<TStreamMeta>&& Streams) override;
	virtual void	ProcessPacket(std::shared_ptr<TMediaPacket> Packet,TStreamWriter& Output) override;
	virtual void	Finish() override	{}

private:
	std::shared_ptr<TStreamBuffer>		mLibavOutput;
	std::shared_ptr<Libav::TContext>	mContext;
	std::shared_ptr<SoyWorkerThread>	mLibavToOutputRelay;
};
