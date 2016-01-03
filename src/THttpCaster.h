#pragma once

#include "TFileCaster.h"


class THttpCaster : public TStreamCaster
{
public:
	THttpCaster(const TCasterParams& Params,std::shared_ptr<Opengl::TContext> OpenglContext);
};


