#include "TBlitter.h"
#include <array.hpp>
#include <SoyOpengl.h>


void Soy::TBlitter::GetGeo(Opengl::TGeometryVertex& Description,ArrayBridge<uint8>&& VertexData,ArrayBridge<size_t>&& Indexes,bool DirectxMode)
{
	//	make mesh
	struct TVertex
	{
		vec4f	uv;	//	gr: dx I think required this to be vec4
	};
	class TMesh
	{
	public:
		TVertex	mVertexes[4];
	};
	TMesh Mesh;
	Mesh.mVertexes[0].uv = vec4f( 0, 0, 0,0);
	Mesh.mVertexes[1].uv = vec4f( 1, 0, 0,0);
	Mesh.mVertexes[2].uv = vec4f( 1, 1, 0,0);
	Mesh.mVertexes[3].uv = vec4f( 0, 1, 0,0);

	Indexes.PushBack( 0 );
	Indexes.PushBack( 1 );
	Indexes.PushBack( 2 );
	
	Indexes.PushBack( 2 );
	Indexes.PushBack( 3 );
	Indexes.PushBack( 0 );
	
	//	for each part of the vertex, add an attribute to describe the overall vertex
	auto& UvAttrib = Description.mElements.PushBack();
	UvAttrib.mName = DirectxMode ? "TEXCOORD" : "TexCoord";	//DX: '0' cannot be part of the name and must be worked out and placed explicitly into the semantic setting 
	UvAttrib.SetType( GL_FLOAT );
	UvAttrib.mIndex = 0;	//	gr: does this matter?	//	gr: use this as semantic number in dx?
	UvAttrib.mArraySize = 4;
	UvAttrib.mElementDataSize = sizeof( Mesh.mVertexes[0].uv );
	
	VertexData.PushBackReinterpret( Mesh );
}



bool Soy::TBlitter::HasWatermark()
{
#if !defined(POPMOVIE_WATERMARK)
#error POPMOVIE_WATERMARK not defined
#elif (POPMOVIE_WATERMARK!=0)
	#if defined(TARGET_WINDOWS)
		#pragma message("Compiling WITH watermark")
	#else
		#warning Compiling WITH watermark
	#endif
	return true;
#else
	#if defined(TARGET_WINDOWS)
		#pragma message("Compiling WITHOUT watermark")
	#else
		#warning Compiling WITHOUT watermark
	#endif
	return false;
#endif
}
