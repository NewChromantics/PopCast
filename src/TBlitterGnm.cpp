#include "TBlitterGnm.h"
#include <SoyPixels.h>

#if !defined(ENABLE_GNM)
#error GNM not enabled
#endif
namespace Renderer = Gnm;


static const uint32_t BlitTest_PixelShader[] = {
#include "BlitTest.pssl.frag.h"
};

static const uint32_t Blit_VertexShader[] = {
#include "Blit.pssl.vert.h"
};


/*
	see samples for abstracted render target
	C:\Program Files (x86)\SCE\ORBIS SDKs\3.500\target\samples\sample_code\input_output_devices\tutorial_vr\common
*/
class Gnm::TRenderTarget
{
public:
	TRenderTarget(TTexture& Target,const TVideoDecoderParams& Params);
	
	SoyPixelsMeta			GetMeta();

	void					Bind(TContext& Context);
	void					Unbind(TContext& Context);

	void					RenderSimpleShader(TVertexShader& VertShader,TPixelShader& FragShader,TGeometry& Geometry,TContext& Context);

private:
	uint32_t				GetSizeInBytes();

public:
	TTexture				mTexture;
	sce::Gnm::RenderTarget	mRenderTarget;
	TVideoDecoderParams		mParams;
};


class Gnm::TGeometry
{
public:
	TGeometry(const ArrayBridge<uint8>&& Data,const ArrayBridge<size_t>&& Indexes,const SoyGraphics::TGeometryVertex& Vertex,TContext& Context);

	void			Draw(TContext& Context,bool ReallyDraw);


	Array<uint8>			mGpuBuffer;
	Array<sce::Gnm::Buffer>	mVertexBuffers;
	size_t					mVertexCount;
};


class Gnm::TShader
{
public:
	TShader(const std::string& Name,const uint32_t* CompiledShader,Gnm::TGpuHeap& Heap);

	virtual void				Bind(TContext& Context)=0;

public:
	std::string					mName;

protected:
	sce::Gnmx::ShaderInfo			mShaderInfo;
	sce::Gnmx::InputResourceOffsets	mResourceOffsets;

	//	allocated on gpu
	Array<uint8>					mGpuShaderBinary;
	Array<uint8>					mGpuShaderHeader;
};

class Gnm::TPixelShader : public TShader
{
public:
	TPixelShader(const std::string& Name,const uint32_t* CompiledShader,Gnm::TGpuHeap& Heap);

	virtual void			Bind(TContext& Context) override;

	sce::Gnmx::PsShader*	GetShader()		{	return reinterpret_cast<sce::Gnmx::PsShader*>( mGpuShaderHeader.GetArray() );	}
};

class Gnm::TVertexShader : public TShader
{
public:
	TVertexShader(const std::string& Name,const uint32_t* CompiledShader,Gnm::TGpuHeap& Heap);

	virtual void			Bind(TContext& Context) override;
	
	sce::Gnmx::VsShader*	GetShader()		{	return reinterpret_cast<sce::Gnmx::VsShader*>( mGpuShaderHeader.GetArray() );	}
};




Gnm::TGeometry::TGeometry(const ArrayBridge<uint8>&& Data,const ArrayBridge<size_t>&& Indexes,const SoyGraphics::TGeometryVertex& Vertex,TContext& Context)
{
	std::Debug << "Allocating geometry..." << std::endl;

	auto& Heap = Context.mHeap;

	mGpuBuffer.SetHeap( Heap );
	mGpuBuffer.Copy( Data );

	auto Alignment = Data.GetDataSize() % Vertex.GetDataSize();
	if ( Alignment != 0 )
	{
		std::stringstream Error;
		Error << "Geometry data size (" << Data.GetDataSize() << ") doesn't align to vertex: " << Vertex.GetDataSize();
		throw Soy::AssertException( Error.str() );
	}
	mVertexCount = Data.GetDataSize() / Vertex.GetDataSize();

	//	each vertex component needs its own buffer for different types
	for ( int i=0;	i<Vertex.mElements.GetSize();	i++ )
	{
		auto& Element = Vertex.mElements[i];
		auto* Address = mGpuBuffer.GetArray() + Vertex.GetOffset(i);
		auto Format = GetFormat( Element.mType );
		
		//	gr: stride, not gap. Let's sort this out!
		auto Stride = Vertex.GetStride(i);
		//auto Stride = Vertex.GetVertexSize();
		
		std::Debug << "Allocating vertex buffer #" << i << ": " << Element << " stride=" << Stride << std::endl;
		sce::Gnm::Buffer Buffer;
		Buffer.initAsVertexBuffer( Address, Format, Stride, mVertexCount );
		Buffer.setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO);

		mVertexBuffers.PushBack( Buffer );
		
		/*
		mVertexBuffer.initAsVertexBuffer( mGpuBuffer.GetArray(), 
		VertexBuffers[0].initAsVertexBuffer((void *)&GPUVertexBuffer->x, sce::Gnm::kDataFormatR32G32B32Float, sizeof(MyVertex), vertexCount);
		VertexBuffers[0].setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO);
		VertexBuffers[1].initAsVertexBuffer((void *)&GPUVertexBuffer->color, sce::Gnm::kDataFormatR8G8B8A8Unorm, sizeof(MyVertex), vertexCount);
		VertexBuffers[1].setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO);
		*/
	}
}


void Gnm::TGeometry::Draw(TContext& Context,bool ReallyDraw)
{
	auto& gfx = Context.GetContext();
	std::Debug << __func__ << " setVertexBuffers(" << mVertexBuffers.GetSize() << ")" << std::endl;
	gfx.setVertexBuffers( sce::Gnm::kShaderStageVs, 0, mVertexBuffers.GetSize(), mVertexBuffers.GetArray() );

	if ( ReallyDraw )
	{
		//	gr: try and organise verts so we dont need an index list
		std::Debug << __func__ << " setPrimitiveType()" << std::endl;
		gfx.setPrimitiveType(sce::Gnm::kPrimitiveTypeTriStrip);

		int VertexCount = mVertexCount;
		VertexCount = 3;
		std::Debug << __func__ << " drawIndexAuto(" << VertexCount << ")" << std::endl;
		gfx.drawIndexAuto(VertexCount);
	}
	else
	{
		std::Debug << "Skipping real draw" << std::endl;
	}
}




Gnm::TShader::TShader(const std::string& Name,const uint32_t* CompiledShader,TGpuHeap& Heap) :
	mName				( Name ),
	mGpuShaderBinary	( static_cast<prmem::Heap&>(Heap) ),
	mGpuShaderHeader	( static_cast<prmem::Heap&>(Heap) )
{
	if ( !CompiledShader )
		throw Soy::AssertException("Null compiled shader provided");

	std::Debug << "Parsing shader " << Name << "..." << std::endl;
	sce::Gnmx::parseShader( &mShaderInfo, CompiledShader );

	if ( mShaderInfo.m_shaderStruct == nullptr )
	{
		std::stringstream Error;
		Error << "Failed to parse shader " << Name;
		throw Soy::AssertException( Error.str() );
	}

}

Gnm::TPixelShader::TPixelShader(const std::string& Name,const uint32_t* CompiledShader,Gnm::TGpuHeap& Heap) :
	TShader	( Name, CompiledShader, Heap )
{
	std::Debug << "Allocating TPixelShader(" << Name << ")" << std::endl;
	//	get source
	auto ShaderBinary = GetRemoteArray( mShaderInfo.m_gpuShaderCode, mShaderInfo.m_gpuShaderCodeSize );
	auto ShaderHeader = GetRemoteArray( reinterpret_cast<uint8*>(mShaderInfo.m_psShader), mShaderInfo.m_psShader->computeSize() );

	//	alloc gpu memory & copy
	mGpuShaderBinary.Copy( ShaderBinary );
	mGpuShaderHeader.Copy( ShaderHeader );

	//	patch so header points at gpu
	auto* GpuShaderPtr = GetShader();
	GpuShaderPtr->patchShaderGpuAddress( mGpuShaderBinary.GetArray() );

	sce::Gnmx::generateInputResourceOffsetTable( &mResourceOffsets, sce::Gnm::kShaderStagePs, GpuShaderPtr );
}

Gnm::TVertexShader::TVertexShader(const std::string& Name,const uint32_t* CompiledShader,Gnm::TGpuHeap& Heap) :
	TShader	( Name, CompiledShader, Heap )
{
	std::Debug << "Allocating TVertexShader(" << Name << ")" << std::endl;
	/*
	sce::Gnmx::parseShader(&VS_info, _binary_VS_sb_start);

	VS_fetchShader = memalign(sce::Gnm::kAlignmentOfFetchShaderInBytes, sce::Gnmx::computeVsFetchShaderSize(VS_info.m_vsShader));	// TODO: this should be garlic memory
	sce::Gnmx::generateVsFetchShader(VS_fetchShader, &VS_shaderModifier, VS_info.m_vsShader, NULL);

	void * gpumem = memalign(sce::Gnm::kAlignmentOfShaderInBytes, VS_info.m_gpuShaderCodeSize);	// TODO: this should be garlic memory
	memcpy(gpumem, VS_info.m_gpuShaderCode, VS_info.m_gpuShaderCodeSize);
	VS_info.m_vsShader->patchShaderGpuAddress(gpumem);

	sce::Gnmx::generateInputResourceOffsetTable(&VS_SRO, sce::Gnm::kShaderStageVs, VS_info.m_vsShader);
*/
	//	get source
	auto ShaderBinary = GetRemoteArray( mShaderInfo.m_gpuShaderCode, mShaderInfo.m_gpuShaderCodeSize );
	auto ShaderHeader = GetRemoteArray( reinterpret_cast<uint8*>(mShaderInfo.m_vsShader), mShaderInfo.m_vsShader->computeSize() );
	//sce::Gnmx::generateVsFetchShader(VS_fetchShader, &VS_shaderModifier, VS_info.m_vsShader, NULL);

	//	alloc gpu memory & copy
	mGpuShaderBinary.Copy( ShaderBinary );
	mGpuShaderHeader.Copy( ShaderHeader );

	//	patch so header points at gpu
	auto* GpuShaderPtr = GetShader();
	GpuShaderPtr->patchShaderGpuAddress( mGpuShaderBinary.GetArray() );

	sce::Gnmx::generateInputResourceOffsetTable( &mResourceOffsets, sce::Gnm::kShaderStageVs, GpuShaderPtr );
}

void Gnm::TVertexShader::Bind(TContext& Context)
{
	std::Debug << "Binding shader " << mName << "..." << std::endl;
	auto& gfx = Context.GetContext();
	auto* Shader = GetShader();
	gfx.setVsShader( Shader, &mResourceOffsets );
}


void Gnm::TPixelShader::Bind(TContext& Context)
{
	std::Debug << "Binding shader " << mName << "..." << std::endl;
	auto& gfx = Context.GetContext();
	auto* Shader = GetShader();
	gfx.setPsShader( Shader, &mResourceOffsets );
}




Gnm::TRenderTarget::TRenderTarget(TTexture& Target,const TVideoDecoderParams& Params) :
	mTexture	( Target ),
	mParams		( Params )
{
	if ( !Target.mTexture->getDataFormat().supportsRenderTarget() )
	{
		throw Soy::AssertException("This texture isn't supported as a render target");
	}

	auto Result = mRenderTarget.initFromTexture( Target.mTexture, 0 );
	if ( Result != 0 )
	{
		std::stringstream Error;
		Error << "Error with mRenderTarget.initFromTexture: " << Result;
		throw Soy::AssertException( Error.str() );
	}

	auto Meta = GetMeta();
	std::Debug << "Successfully created rendertarget from texture " << Meta << std::endl;
}

SoyPixelsMeta Gnm::TRenderTarget::GetMeta()
{
	auto Width = mRenderTarget.getWidth();
	auto Height = mRenderTarget.getHeight();
	return SoyPixelsMeta( Width, Height, SoyPixelsFormat::Invalid );
}

void Gnm::TRenderTarget::Bind(TContext& Context)
{
	std::Debug << __func__ << std::endl;
	auto& gfxc = Context.GetContext();

	// Clear depth
	gfxc.pushMarker( __func__, 0x00000000);

	gfxc.popMarker();

	// Set render target
	gfxc.setRenderTarget(0, &mRenderTarget);
	gfxc.setDepthRenderTarget(nullptr);
	gfxc.setRenderTargetMask(0xF);

	/*
	if(m_msaa)
	{
		Gnm::DepthEqaaControl eqaaReg;
		eqaaReg.init();
		eqaaReg.setMaxAnchorSamples(sce::Gnm::kNumSamples4);
		eqaaReg.setAlphaToMaskSamples(sce::Gnm::kNumSamples4);
		eqaaReg.setStaticAnchorAssociations(true);
		gfxc.setDepthEqaaControl(eqaaReg);

		// Set up Gnmx::GfxContext.
		gfxc.setScanModeControl(sce::Gnm::kScanModeControlAaEnable, Gnm::kScanModeControlViewportScissorEnable);
		gfxc.setAaDefaultSampleLocations(sce::Gnm::kNumSamples4);
	}
	else
	{
		gfxc.setScanModeControl(sce::Gnm::kScanModeControlAaDisable, sce::Gnm::kScanModeControlViewportScissorEnable);
		gfxc.setAaDefaultSampleLocations(sce::Gnm::kNumSamples1);
	}
	*/

	// Set up viewport and projection
	gfxc.setupScreenViewport(0, 0, mRenderTarget.getWidth(), mRenderTarget.getHeight(), 0.5f, 0.5f);
}

uint32_t Gnm::TRenderTarget::GetSizeInBytes()
{
	return mRenderTarget.getSliceSizeInBytes()*(mRenderTarget.getLastArraySliceIndex()-mRenderTarget.getBaseArraySliceIndex()+1);
}

void Gnm::TRenderTarget::Unbind(TContext& Context)
{
	std::Debug << __func__ << std::endl;
	//auto& gfxc = Context.GetContext();

	//	restore backbuffer?
	//if ( mParams.mWin7Emulation )
	//	gfxc.setRenderTarget(0, nullptr );
	/*
	// Sync on the offscreen target
	gfxc.waitForGraphicsWrites( mRenderTarget.getBaseAddress256ByteBlocks(), GetSizeInBytes()>>8,
							   sce::Gnm::kWaitTargetSlotCb0, sce::Gnm::kCacheActionWriteBackAndInvalidateL1andL2, sce::Gnm::kExtendedCacheActionFlushAndInvalidateCbCache,
							   sce::Gnm::kStallCommandBufferParserDisable);
	/  *
	if( m_msaa )
	{
		int beginSlice = m_resolvedRenderTarget.getBaseArraySliceIndex();
		int endSlice = m_resolvedRenderTarget.getLastArraySliceIndex();
		if(beginSlice != endSlice){
			for(int i = beginSlice; i <= endSlice; i++){
				m_resolvedRenderTarget.setArrayView(i, i);
				m_renderTarget.setArrayView(i, i);
				Gnmx::hardwareMsaaResolve(&gfxc, &m_resolvedRenderTarget, &m_renderTarget);
			}
			m_resolvedRenderTarget.setArrayView(beginSlice, endSlice);
			m_renderTarget.setArrayView(beginSlice, endSlice);
		}else{
			Gnmx::hardwareMsaaResolve(&gfxc, &m_resolvedRenderTarget, &m_renderTarget);
		}
		gfxc.setScanModeControl(Gnm::kScanModeControlAaDisable, Gnm::kScanModeControlViewportScissorEnable);
		gfxc.setAaDefaultSampleLocations(Gnm::kNumSamples1);
	}
	*/
}


//	from toolkit clear
void Gnm::TRenderTarget::RenderSimpleShader(TVertexShader& VertShader,TPixelShader& FragShader,TGeometry& Geometry,TContext& Context)
{
	std::Debug << __func__ << std::endl;

	auto& gfx = Context.GetContext();

	gfx.setShaderType(sce::Gnm::kShaderTypeGraphics);
	gfx.setActiveShaderStages(sce::Gnm::kActiveShaderStagesVsPs);
	if ( mParams.mDecoderUseHardwareBuffer )
	{
		VertShader.Bind( Context );
		FragShader.Bind( Context );
	}
	Geometry.Draw( Context, mParams.mBlitMergeYuv );

	
	/*

	const bool srcUintsIsPowerOfTwo = (srcUints & (srcUints-1)) == 0;

	gfxc.setCsShader(srcUintsIsPowerOfTwo ? s_set_uint_fast.m_shader : s_set_uint.m_shader, 
	srcUintsIsPowerOfTwo ? &s_set_uint_fast.m_offsetsTable : &s_set_uint.m_offsetsTable);

	Gnm::Buffer destinationBuffer;
	destinationBuffer.initAsDataBuffer(destination, Gnm::kDataFormatR32Uint, destUints);
	destinationBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeGC);
	gfxc.setRwBuffers(Gnm::kShaderStageCs,t 0, 1, &destinationBuffer);

	Gnm::Buffer sourceBuffer;
	sourceBuffer.initAsDataBuffer(source, Gnm::kDataFormatR32Uint, srcUints);
	sourceBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
	gfxc.setBuffers(Gnm::kShaderStageCs, 0, 1, &sourceBuffer);

	struct Constants
	{
	uint32_t m_destUints;
	uint32_t m_srcUints;
	};
	Constants *constants = (Constants*)gfxc.allocateFromCommandBuffer(sizeof(Constants), Gnm::kEmbeddedDataAlignment4);
	constants->m_destUints = destUints;
	constants->m_srcUints = srcUints - (srcUintsIsPowerOfTwo ? 1 : 0);
	Gnm::Buffer constantBuffer;
	constantBuffer.initAsConstantBuffer(constants, sizeof(*constants));
	gfxc.setConstantBuffers(Gnm::kShaderStageCs, 0, 1, &constantBuffer);

	gfxc.dispatch((destUints + Gnm::kThreadsPerWavefront - 1) / Gnm::kThreadsPerWavefront, 1, 1);

	Toolkit::synchronizeComputeToGraphics(&gfxc.m_dcb);
	*/
}


/*
NewFrameBuffer()
{

	//E Initialization of graphics
	static int initGraphics( uint32_t displayWidth, uint32_t displayHeight,
							sce::Gnm::DataFormat format, sce::Gnm::TileMode* tmode )
	{
		sce::Gnm::TileMode tileMode;
		uint32_t bufferAlign = 256;

		//E Allocate the memory used for graphics processing
		g_graphicsMemory = graphicsMemoryInit( &s_graphicsMemory,
											  80*1024*1024, 2*1024*1024, SCE_KERNEL_WC_GARLIC );
		if (! g_graphicsMemory ) {
			return -1;
		}

		//E Preparation of render target
		if ( tmode ) {
			uint32_t numElementPerPixel = 1;

			int ret = sce::GpuAddress::computeSurfaceTileMode( sce::Gnm::getGpuMode(),
															  &tileMode,
															  sce::GpuAddress::kSurfaceTypeColorTargetDisplayable,
															  format,
															  numElementPerPixel );
			if ( ret != SCE_OK ) {
				return ret;
			}
			*tmode = tileMode;
		}
		else {
			tileMode = sce::Gnm::kTileModeDisplay_LinearAligned;
		}
		{
			sce::Gnm::RenderTargetSpec targetSpec;
			int ret;

			targetSpec.init();
			targetSpec.m_width  = displayWidth;
			targetSpec.m_height = displayHeight;
			targetSpec.m_pitch  = 0;
			targetSpec.m_numSlices = 1;
			targetSpec.m_colorFormat = format;
			targetSpec.m_colorTileModeHint = tileMode;
			targetSpec.m_minGpuMode = sce::Gnm::getGpuMode();
			targetSpec.m_numSamples = sce::Gnm::kNumSamples1;
			targetSpec.m_numFragments = sce::Gnm::kNumFragments1;
			targetSpec.m_flags.enableCmaskFastClear   = 0;
			targetSpec.m_flags.enableFmaskCompression = 0;

			ret = 
		}
		*/



Gnm::TBlitter::TBlitter(const TVideoDecoderParams& Params) :
	mParams	( Params )
{
}

Gnm::TBlitter::~TBlitter()
{
}

void Gnm::TBlitter::BlitTexture(TTexture& Target,ArrayBridge<TTexture>&& Source,TContext& Context)
{
	throw Soy::AssertException("Currently using blittest or BlitUnityTest");
}


//#define USE_UNITY_SHADERS

#if defined(USE_UNITY_SHADERS)
//extern "C" char _binary_VS_sb_start[];
//extern "C" char _binary_PS_sb_start[];
//const char* _binary_VS_sb_start = reinterpret_cast<const char*>( &Blit_VertexShader[0] );
//const char* _binary_PS_sb_start = reinterpret_cast<const char*>( &BlitTest_PixelShader[0] );
sce::Gnmx::ShaderInfo	VS_info, PS_info;
sce::Gnmx::InputResourceOffsets VS_SRO, PS_SRO;
void *VS_fetchShader = NULL;
uint32_t VS_shaderModifier = 0;
#endif

#if defined(USE_UNITY_SHADERS)
void EnsurePS4ResourcesAreCreated(Gnm::TContext& Context)
{
	static bool initialsied = false;
	if ( initialsied )
		return;

	auto GpuAlloc = [&](int Alignment,size_t Size)->void*
	{
		auto& Heap = Context.mHeap;
		return Heap.Alloc<uint8>( Size );
	};
	
	{
		std::Debug << __func__ << " parseShader PS" << std::endl;
		sce::Gnmx::parseShader(&PS_info, BlitTest_PixelShader);
		if ( PS_info.m_psShader == nullptr )
		{
			throw Soy::AssertException("Failed to allocate PS shader");
		}
		std::Debug << __func__ << " PS Shader @" << reinterpret_cast<uintptr_t>( PS_info.m_psShader ) << std::endl;
		std::Debug << __func__ << " PS Shader compute size=" << PS_info.m_psShader->computeSize() << std::endl;

		std::Debug << __func__ << " GpuAlloc m_gpuShaderCodeSize=" << PS_info.m_gpuShaderCodeSize << std::endl;
		void * gpumem = GpuAlloc(256, PS_info.m_gpuShaderCodeSize);	// TODO: this should be garlic memory
		if ( gpumem == nullptr )
			throw Soy::AssertException("Failed to  PS_info.m_gpuShaderCodeSize");
		memcpy(gpumem, PS_info.m_gpuShaderCode, PS_info.m_gpuShaderCodeSize);
		std::Debug << __func__ << " patchShaderGpuAddress PS" << std::endl;
		if ( PS_info.m_psShader->m_psStageRegisters.m_spiShaderPgmHiPs == 0xFFFFFFFF )
			std::Debug << "PsShader gpu address has already been patched." << std::endl;
		else
			PS_info.m_psShader->patchShaderGpuAddress(gpumem);

		std::Debug << __func__ << " ps generateInputResourceOffsetTable" << std::endl;
		sce::Gnmx::generateInputResourceOffsetTable(&PS_SRO, sce::Gnm::kShaderStagePs, PS_info.m_vsShader);

	}

	{
		std::Debug << __func__ << " sce::Gnmx::parseShader(vs) " << std::endl;
		sce::Gnmx::parseShader(&VS_info, Blit_VertexShader);

		if ( VS_info.m_vsShader == nullptr )
		{
			throw Soy::AssertException("Failed to allocate VS shader");
		}

		std::Debug << __func__ << " memalign fetch " << std::endl;
		VS_fetchShader = GpuAlloc(sce::Gnm::kAlignmentOfFetchShaderInBytes, sce::Gnmx::computeVsFetchShaderSize(VS_info.m_vsShader));	// TODO: this should be garlic memory
		std::Debug << __func__ << " generateVsFetchShader " << std::endl;
		sce::Gnmx::generateVsFetchShader(VS_fetchShader, &VS_shaderModifier, VS_info.m_vsShader, NULL);

		std::Debug << __func__ << " memalign gpu shader " << std::endl;
		void * gpumem = GpuAlloc(sce::Gnm::kAlignmentOfShaderInBytes, VS_info.m_gpuShaderCodeSize);	// TODO: this should be garlic memory
		memcpy(gpumem, VS_info.m_gpuShaderCode, VS_info.m_gpuShaderCodeSize);
		std::Debug << __func__ << " patchShaderGpuAddress" << std::endl;
		VS_info.m_vsShader->patchShaderGpuAddress(gpumem);

		std::Debug << __func__ << " generateInputResourceOffsetTable" << std::endl;
		sce::Gnmx::generateInputResourceOffsetTable(&VS_SRO, sce::Gnm::kShaderStageVs, VS_info.m_vsShader);
	}


	initialsied = true;
}
#endif


#include <vectormath.h>

typedef sce::Vectormath::Scalar::Aos::Point3 Point3;

typedef sce::Vectormath::Scalar::Aos::Vector2 Vector2;
typedef sce::Vectormath::Scalar::Aos::Vector3 Vector3;
typedef sce::Vectormath::Scalar::Aos::Vector4 Vector4;

typedef sce::Vectormath::Scalar::Aos::Quat Quat;

typedef sce::Vectormath::Scalar::Aos::Matrix3 Matrix3;
typedef sce::Vectormath::Scalar::Aos::Matrix4 Matrix4;
#include <vectormath.h>

// Vector2Unaligned is meant to store a Vector2, except that padding and alignment are
// 4-byte granular (GPU), rather than 16-byte (SSE). This is to bridge
// the SSE math library with GPU structures that assume 4-byte granularity.
// While a Vector2 has many operations, Vector2Unaligned can only be converted to and from Vector2.
struct Vector2Unaligned
{
	float x;
	float y;
};

inline Vector2Unaligned ToVector2Unaligned( const Vector2& r )
{
	const Vector2Unaligned result = { r.getX(), r.getY() };
	return result;
}

inline Vector2 ToVector2( const Vector2Unaligned& r )
{
	return Vector2( r.x, r.y );
}




struct MyVertex {
	float x, y, z;
	unsigned int color;
};



void Gnm::TBlitter::BlitUnityTest(TTexture& Target,TContext& Context)
{

	std::Debug << __func__ << std::endl;
#if defined(USE_UNITY_SHADERS)
	EnsurePS4ResourcesAreCreated(Context);
#else
	auto& vs = GetVertShader(Context);
	auto& ps = GetFragShader(Context);
#endif

	// A colored triangle. Note that colors will come out differently
	// in D3D9/11 and OpenGL, for example, since they expect color bytes
	// in different ordering.
	MyVertex verts[3] = {
		{ -0.5f, -0.25f,  0, 0xFFff0000 },
		{  0.5f, -0.25f,  0, 0xFF00ff00 },
		{  0,     0.5f ,  0, 0xFF0000ff },
	};


	// Some transformation matrices: rotate around Z axis for world
	// matrix, identity view matrix, and identity projection matrix.

	float phi = 0;//g_Time;
	float cosPhi = cosf(phi);
	float sinPhi = sinf(phi);

	float worldMatrix[16] = {
		cosPhi,-sinPhi,0,0,
		sinPhi,cosPhi,0,0,
		0,0,1,0,
		0,0,0.7f,1,
	};
	/*
	float identityMatrix[16] = {
		1,0,0,0,
		0,1,0,0,
		0,0,1,0,
		0,0,0,1,
	};
	*/
	float projectionMatrix[16] = {
		1,0,0,0,
		0,1,0,0,
		0,0,1,0,
		0,0,0,1,
	};


	auto* gfxc = &Context.GetContext();
//	IUnityGraphicsPS4* iPS4 = s_UnityInterfaces->Get<IUnityGraphicsPS4>();
//	sce::Gnmx::LightweightGfxContext *gfxc = (sce::Gnmx::LightweightGfxContext *)iPS4->GetGfxContext();


	//sce::Gnm::RenderTarget *renderTarget = (sce::Gnm::RenderTarget *)iPS4->GetCurrentRenderTarget(0);
	//sce::Gnm::DepthRenderTarget *depthRenderTarget = (sce::Gnm::DepthRenderTarget *)iPS4->GetCurrentDepthRenderTarget();

	class ShaderConstants
	{
	public:
		float m_worldMatrix[16];
		float m_projectionMatrix[16];
	};

	int vertexCount = 3;
	const int numBuffers = 2;
	static sce::Gnm::Buffer VertexBuffers[numBuffers];

	// Tweak the projection matrix a bit to make it match what identity projection would do in D3D case.
	projectionMatrix[10] = 2.0f;
	projectionMatrix[14] = -1.0f;

	sce::Gnm::Buffer constBuffer;
	static MyVertex *GPUVertexBuffer = NULL;
	if (GPUVertexBuffer == NULL)
	{
		std::Debug << __func__ << "Allocating GPU vertexes" << std::endl;

		// The vertices need to be in allocated GPU mem, but as the vertices aren't changing in this demo we can just copy them the first time
		int buffersize = vertexCount  * sizeof(MyVertex);
		GPUVertexBuffer = (MyVertex *)Context.mHeap.AllocArray<uint8>(buffersize);
		//iPS4->AllocateGPUMemory(buffersize, sce::Gnm::kAlignmentOfBufferInBytes);
		memcpy(GPUVertexBuffer, verts, buffersize);

		std::Debug << __func__ << "vertex buffer inits" << std::endl;
		VertexBuffers[0].initAsVertexBuffer((void *)&GPUVertexBuffer->x, sce::Gnm::kDataFormatR32G32B32Float, sizeof(MyVertex), vertexCount);
		VertexBuffers[0].setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO);
		VertexBuffers[1].initAsVertexBuffer((void *)&GPUVertexBuffer->color, sce::Gnm::kDataFormatR8G8B8A8Unorm, sizeof(MyVertex), vertexCount);
		VertexBuffers[1].setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO);

		std::Debug << __func__ << "vertex buffer inits done" << std::endl;
		//	printf("renderTarget %dx%d\n", renderTarget->getWidth(), renderTarget->getHeight());

	}

	auto& RenderTarget = GetRenderTarget(Target);
	if ( mParams.mWindowIncludeBorders )
	{
		std::Debug << __func__ << " binding rendertarget" << std::endl;
		RenderTarget.Bind( Context );
	}


	gfxc->pushMarker("Native Rendering Plugin", 0x00ff00);

	std::Debug << __func__ << " setActiveShaderStages" << std::endl;
	gfxc->setShaderType(sce::Gnm::kShaderTypeGraphics);
	gfxc->setActiveShaderStages(sce::Gnm::kActiveShaderStagesVsPs);

#if defined(USE_UNITY_SHADERSsetConstantBuffers)
	std::Debug << __func__ << " setVsShader" << std::endl;
	gfxc->setVsShader(VS_info.m_vsShader, VS_shaderModifier, VS_fetchShader, &VS_SRO);
	std::Debug << __func__ << " setPsShader" << std::endl;
	gfxc->setPsShader(PS_info.m_psShader, &PS_SRO);
#else
	std::Debug << __func__ << " vs.bind();" << std::endl;
	vs.Bind(Context);
	std::Debug << __func__ << " ps.bind();" << std::endl;
	ps.Bind(Context);
#endif

	if ( mParams.mWin7Emulation )
	{
		// as the shader constants change every frame we need to continually allocate memory for it ... using the command buffer is ideal
		std::Debug << __func__ << " allocateFromCommandBuffer" << std::endl;
		ShaderConstants *constants = (ShaderConstants*)gfxc->allocateFromCommandBuffer(sizeof(ShaderConstants), sce::Gnm::kEmbeddedDataAlignment4);
		std::Debug << __func__ << " constants allocated as " << reinterpret_cast<uintptr_t>(constants) << std::endl;
		memcpy(&constants->m_projectionMatrix, projectionMatrix, sizeof(constants->m_projectionMatrix));
		memcpy(&constants->m_worldMatrix, worldMatrix, sizeof(constants->m_worldMatrix));
		constBuffer.initAsConstantBuffer(constants, sizeof(ShaderConstants));
		std::Debug << __func__ << " setConstantBuffers" << std::endl;
		gfxc->setConstantBuffers(sce::Gnm::kShaderStageVs, 0, 1, &constBuffer);
	}

	std::Debug << __func__ << " setVertexBuffers" << std::endl;
	gfxc->setVertexBuffers(sce::Gnm::kShaderStageVs, 0, numBuffers, VertexBuffers);

	gfxc->setPrimitiveType(sce::Gnm::kPrimitiveTypeTriList);
	std::Debug << __func__ << " drawIndexAuto" << std::endl;
	gfxc->drawIndexAuto(3);

	gfxc->popMarker();

	/*
	// update native texture from code
	if (g_TexturePointer)
	{
		sce::Gnm::Texture *Texture = (sce::Gnm::Texture *)(g_TexturePointer);

		// convert texture so that it will be rendered linearly ... this avoids having to re-tile it (but renders slower)
		Texture->setTileMode(sce::Gnm::kTileModeDisplay_LinearAligned);

		FillTextureFromCode(Texture->getWidth(), Texture->getHeight(), Texture->getHeight() * 4, (unsigned char *)Texture->getBaseAddress());
	}
	*/
	if ( mParams.mWindowIncludeBorders )
	{
		std::Debug << __func__ << " RenderTarget.Unbind" << std::endl;
		RenderTarget.Unbind( Context );
	}


}

void Gnm::TBlitter::BlitTest(TTexture& Target,TContext& Context)
{
	std::Debug << __func__ << std::endl;
	
	auto& RenderTarget = GetRenderTarget(Target);

	RenderTarget.Bind( Context );

	auto Cleanup = [&]
	{
		RenderTarget.Unbind( Context );
	};

	try
	{
		if ( mParams.mWindowIncludeBorders )
		{
			auto& VertShader = GetVertShader( Context );
			auto& FragShader = GetFragShader( Context );
			auto& Geometry = GetGeometry( Context );

			if ( mParams.mApplyHeightPadding )
				RenderTarget.RenderSimpleShader( VertShader, FragShader, Geometry, Context );
		}

		Cleanup();
	}
	catch(...)
	{
		Cleanup();
		throw;
	}

	//gfx.setRenderTarget( 0 )
	/*
	auto& gfx = *Context.mContext;

	gfx.setRenderTargetMask( 0xf );
	gfx.setDepthStencilDisable();

	//	gr: "auto" geometry... but not really very specific about it
	gfx.setPrimitiveType( sce::Gnm::kPrimitiveTypeTriStrip );
	gfx.drawIndexAuto( 4 );
	*/

	// Add completion event
	//textures->marker_wait[textures->bink_buffers.FrameNum] = ++shaders->marker;
	//Gfx->writeImmediateAtEndOfPipe( Gnm::kEopCbDbReadsDone, (void *)shaders->CompletionEvent, textures->marker_wait[textures->bink_buffers.FrameNum], Gnm::kCacheActionNone );

	/*
	BINKTEXTURESPS4 * textures = (BINKTEXTURESPS4*) ptextures;
	BINKSHADERSPS4 * shaders = (BINKSHADERSPS4*) pshaders;
	Gnmx::GfxContext * Gfx = (Gnmx::GfxContext *) Gfxcontext;

	if ( shaders == 0 )
		shaders = textures->shaders;

	Gnm::Sampler Sampler;
	Sampler.init();
	Sampler.setXyFilterMode( Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear );
	Sampler.setWrapMode( Gnm::kWrapModeClampLastTexel, Gnm::kWrapModeClampLastTexel, Gnm::kWrapModeClampLastTexel );

	Gfx->setRenderTargetMask( 0xf );
	Gfx->setDepthStencilDisable();

	Gnm::Buffer buf;
	F32 *consts = (F32*) Gfx->allocateFromCommandBuffer( 6*4*4, Gnm::kEmbeddedDataAlignment4 );
	buf.initAsConstantBuffer( consts, 6*4*4 );
	Gfx->setConstantBuffers( Gnm::kShaderStageVs, 0, 1, &buf );
	Gfx->setConstantBuffers( Gnm::kShaderStagePs, 0, 1, &buf );

	// Pixel shader alpha constants
	consts[ 0 ] = ( textures->is_pm ) ? textures->alpha : 1.0f;
	consts[ 1 ] = consts [ 0 ];
	consts[ 2 ] = consts [ 0 ];
	consts[ 3 ] = textures->alpha;

	// set the constants for the type of ycrcb we have
	memcpy( consts + 4, textures->bink->ColorSpace, sizeof( textures->bink->ColorSpace ) );

	// Vertex shader constants
	consts[ 20 ] = ( textures->x1 - textures->x0 ) * 2.0f;
	consts[ 21 ] = ( textures->y1 - textures->y0 ) * -2.0f; // view space has +y = up, our coords have +y = down
	consts[ 22 ] = textures->x0 * 2.0f - 1.0f;
	consts[ 23 ] = 1.0f - textures->y0 * 2.0f;

	// Set up blending.
	Gnm::BlendControl blend;
	blend.init();
	if ( textures->bink_buffers.Frames[0].APlane.Allocate || ( textures->alpha < 0.999f ) )
	{
		blend.setBlendEnable( true );
		blend.setColorEquation( textures->is_pm ? Gnm::kBlendMultiplierOne : Gnm::kBlendMultiplierSrcAlpha, Gnm::kBlendFuncAdd, Gnm::kBlendMultiplierOneMinusSrcAlpha );
	}
	Gfx->setBlendControl(0, blend);

	Gfx->setVsShader( shaders->VertexShader.vs, 0, (void*)NULL );

	Gfx->setTextures( Gnm::kShaderStagePs, 0, 1, (const Gnm::Texture*)textures->Ytexture[textures->bink_buffers.FrameNum] );
	Gfx->setTextures( Gnm::kShaderStagePs, 1, 1, (const Gnm::Texture*)textures->cRtexture[textures->bink_buffers.FrameNum] );
	Gfx->setTextures( Gnm::kShaderStagePs, 2, 1, (const Gnm::Texture*)textures->cBtexture[textures->bink_buffers.FrameNum] );
	Gfx->setSamplers( Gnm::kShaderStagePs, 0, 1, &Sampler );

	if (textures->bink_buffers.Frames[0].APlane.Allocate)
	{
		Gfx->setTextures( Gnm::kShaderStagePs, 3, 1, (const Gnm::Texture*)textures->Atexture[textures->bink_buffers.FrameNum] );
		Gfx->setPsShader( shaders->PixelShader[1].ps );
	}
	else
		Gfx->setPsShader( shaders->PixelShader[0].ps );

	// Add the draw
	Gfx->setPrimitiveType( Gnm::kPrimitiveTypeTriStrip );
	Gfx->drawIndexAuto( 4 );

	// Add completion event
	textures->marker_wait[textures->bink_buffers.FrameNum] = ++shaders->marker;
	Gfx->writeImmediateAtEndOfPipe( Gnm::kEopCbDbReadsDone, (void *)shaders->CompletionEvent, textures->marker_wait[textures->bink_buffers.FrameNum], Gnm::kCacheActionNone );

	*/
}


Gnm::TRenderTarget& Gnm::TBlitter::GetRenderTarget(TTexture& Target)
{
	if ( mRenderTarget )
	{
		if ( mRenderTarget->mTexture != Target )
			mRenderTarget.reset();
	}

	if ( !mRenderTarget )
	{
		mRenderTarget.reset( new TRenderTarget( Target, mParams ) );
	}

	return *mRenderTarget;
}

Gnm::TVertexShader& Gnm::TBlitter::GetVertShader(TContext& Context)
{
	if ( !mVertShader )
	{
		mVertShader.reset( new TVertexShader("Blit_VertexShader",Blit_VertexShader, Context.mHeap ) );
	}
	return *mVertShader;
}

Gnm::TPixelShader& Gnm::TBlitter::GetFragShader(TContext& Context)
{
	if ( !mFragShader )
	{
		mFragShader.reset( new TPixelShader("BlitTest_PixelShader", BlitTest_PixelShader, Context.mHeap ) );
	}
	return *mFragShader;

}

Gnm::TGeometry& Gnm::TBlitter::GetGeometry(TContext& Context)
{
	if ( !mGeometry )
	{
		SoyGraphics::TGeometryVertex Description;
		Array<uint8> VertexData;
		Array<size_t> Indexes;
		bool DirectXMode = false;

		GetGeo( Description, GetArrayBridge(VertexData), GetArrayBridge(Indexes), DirectXMode );

		mGeometry.reset( new TGeometry( GetArrayBridge(VertexData), GetArrayBridge(Indexes), Description, Context ) );
	}
	return *mGeometry;
}
