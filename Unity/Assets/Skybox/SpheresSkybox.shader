Shader "NewChromantics/Spheres"
{
    Properties
    {
    	_MainTex ("Texture", 2D) = "white" {}
    	SphereRadius("Radius", Range(0.01,1) ) = 0.09
    	SpherePosition("SpherePosition", VECTOR ) = (0.5,0.5,0.5)
   		GridSize("GridSize", Range(0,20) ) = 4.5
 		ColourGridSize("ColourGridSize", Range(0,1) ) = 0.9

    	CameraX("CameraX", Range(-1,1) ) = 0
    	CameraY("CameraY", Range(-1,1) ) = 0
    	CameraZ("CameraZ", Range(-1,1) ) = 0

    	RayStep("RayStep", Range(0,0.1) ) = 0.009
    	RayStepGrowth("RayStepGrowth", Range(0,0.1) ) = 0.034
    }

    CGINCLUDE

    #include "UnityCG.cginc"

    struct appdata
    {
        float4 position : POSITION;
        float3 texcoord : TEXCOORD0;
    };
    
    struct v2f
    {
        float4 position : SV_POSITION;
        float3 texcoord : TEXCOORD0;
        float3 worldpos : TEXCOORD1;
    };
    
    sampler2D _MainTex;
 
	#include "PopCommon.cginc"

	float SphereRadius;
	float3 SpherePosition;
	float GridSize;
	float ColourGridSize;
	float CameraX;
	float CameraY;
	float CameraZ;
	float RayStep;
	float RayStepGrowth;
	 
    v2f vert (appdata v)
    {
        v2f o;
        o.position = mul (UNITY_MATRIX_MVP, v.position);
        o.worldpos = v.position;
        o.texcoord = v.texcoord;
        return o;
    }


    float4 Nearest(float4 a,float4 b)
    {
    	if ( a.w < b.w )
    		return a;
    	else
    		return b;
    }

    //	get an object, d
    float4 GetObject_Sphere(float3 RayPos,float3 RayDir,float3 Colour)
    {
    	//	chunk this world space pos into a grid
    	float3 GridIndex = floor( RayPos / GridSize );
    	float3 GridLocal = (RayPos / GridSize) - GridIndex;


		float ColourGridSizeNorm = ColourGridSize * GridSize;//	stop dupe
    	Colour = (RayPos / ColourGridSizeNorm) - floor( RayPos / ColourGridSizeNorm );
    	Colour = normalize(Colour);

    	float SphereDist = distance( GridLocal, SpherePosition ) - SphereRadius;
    	return float4( Colour.x, Colour.y, Colour.z, SphereDist );
    }

    float4 GetBackgroundColour()
    {
    #define BACKGROUND_COLOUR	float4( 0,0,0,1 )
    	return BACKGROUND_COLOUR;
    }

    fixed4 frag (v2f i) : COLOR
    {
    	//	take the ray and figure out if it intersects a virtual sphere
    	float3 RayDir = normalize(i.texcoord);
    	float3 RayPos = _WorldSpaceCameraPos;
    	//float3 RayPos = i.worldpos;


		float4 Object = float4(0,0,0,9999);
		#define RAY_COUNT	200
		for( int i=0;	i<RAY_COUNT;	i++ )
		{
			float RayStepNorm = i/(float)RAY_COUNT;

			float3 Colour = 1-RayStepNorm;
			float4 Sphere = GetObject_Sphere( RayPos, RayDir, Colour );

			Object = Nearest( Sphere, Object );

			//	hit something
			if ( Object.w <= 0 )
				break;

			RayPos += RayDir * RayStep;
   			//	make the step larger as we go into the distance
			RayStep += RayStep*RayStepGrowth;
		}

    	if ( Object.w < 0 )
    	{
    		return float4( Object.x, Object.y, Object.z, 1 );
    	}
    	else
    	{
    		return GetBackgroundColour();
    	}
    }

    ENDCG

    SubShader
    {
        Tags { "RenderType"="Background" "Queue"="Background" }
        Pass
        {
            ZWrite Off
            Cull Off
            Fog { Mode Off }
            CGPROGRAM
            #pragma fragmentoption ARB_precision_hint_fastest
            #pragma vertex vert
            #pragma fragment frag
            ENDCG
        }
    }
}
