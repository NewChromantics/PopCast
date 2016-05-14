#define PIf	3.1415926535897932384626433832795f

float RadToDeg(float Rad)
{
	return Rad * (360.0f / (PIf * 2.0f) );
}

float DegToRad(float Deg)
{
	return Deg * ((PIf * 2.0f)/360.0f );
}

 
float Range(float Min,float Max,float Time)
{
	return (Time-Min) / (Max-Min);
}

//	y = visible
float2 RescaleFov(float Radians,float MinDegrees,float MaxDegrees)
{
	float Deg = RadToDeg( Radians );
	Deg += 180;
	
	//	re-scale
	Deg = Range( MinDegrees, MaxDegrees, Deg );
	bool Visible = Deg >= 0 && Deg <= 1;
	Deg *= 360;
	
	Deg -= 180;
	Radians = DegToRad( Deg );
	
	return float2( Radians, Visible?1:0 );
}

float3 ViewToLatLonVisible(float3 View3,float2 CameraFovMin,float2 CameraFovMax)
{
	View3 = normalize(View3);
	//	http://en.wikipedia.org/wiki/N-vector#Converting_n-vector_to_latitude.2Flongitude
	float x = View3.x;
	float y = View3.y;
	float z = View3.z;

//	auto lat = tan2( x, z );
	float lat = atan2( x, z );
	
	//	normalise y
	float xz = sqrt( (x*x) + (z*z) );
	float normy = y / sqrt( (y*y) + (xz*xz) );
	float lon = sin( normy );
	//float lon = atan2( y, xz );
	
	//	stretch longitude...
	//	gr: removed this as UV was wrapping around
	//lon *= 2.0;
	//	gr: sin output is -1 to 1...
	lon *= PIf;
	
	bool Visible = true;
	float2 NewLat = RescaleFov( lat, CameraFovMin.x, CameraFovMax.x );
	float2 NewLon = RescaleFov( lon, CameraFovMin.y, CameraFovMax.y );
	lat = NewLat.x;
	lon = NewLon.x;
	Visible = (NewLat.y*NewLon.y) > 0;
	

	return float3( lat, lon, Visible ? 1:0 );
}

float2 LatLonToUv(float2 LatLon,float Width,float Height)
{
	//	-pi...pi -> -1...1
	float lat = LatLon.x;
	float lon = LatLon.y;
	
	lat /= PIf;
	lon /= PIf;
	
	//	-1..1 -> 0..2
	lat += 1.0;
	lon += 1.0;
	
	//	0..2 -> 0..1
	lat /= 2.0;
	lon /= 2.0;
					
	lat *= Width;
	lon *= Height;
	
	return float2( lat, lon );
}

//	3D view normal to equirect's UV. Z is 0 if invalid
float3 ViewToUvVisible(float3 ViewDir,float2 CameraFovMin,float2 CameraFovMax)
{
	float3 latlonVisible = ViewToLatLonVisible( ViewDir, CameraFovMin, CameraFovMax );
	latlonVisible.xy = LatLonToUv( latlonVisible.xy, 1, 1 );
	return latlonVisible;
}

float2 ViewToUv(float3 View3)
{
	float3 UvVisible = ViewToUvVisible( View3, float2(0,0), float2(1,1) );
	return UvVisible.xy;
}
    
 