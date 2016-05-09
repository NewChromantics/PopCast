using UnityEngine;
using System.Collections;
using System.Collections.Generic;

public class PopCastCameraCapture : MonoBehaviour
{
	public enum PopCaptureMode
	{
		Camera,
		Cubemap,
		Equirect360
	};

	public PopCastParams Params;
	public PopCast Cast = null;
	public string OutputTarget = "file:StreamingAssets/*.gif";
	public Camera CaptureCamera;
	public PopCaptureMode CaptureMode;
	public RenderTexture CaptureCameraTexture;
	public bool EnableDebug = true;

	void OnEnable()
	{
		if (Cast == null)
		{
			Cast = new PopCast( OutputTarget, Params);
			if (EnableDebug)
				PopCast.EnableDebugLog = EnableDebug;
		}
	}

	void OnDisable()
	{
		if ( Cast != null )
		{
			Cast.Free();
			Cast = null;
		}
	}

void CorrectRenderTextureSizeForMp4(ref int Width,ref int Height)
{
//	must align, or something
if (!Mathf.IsPowerOfTwo (Width))
Width = Mathf.NextPowerOfTwo (Width);
if (!Mathf.IsPowerOfTwo (Height))
Height = Mathf.NextPowerOfTwo (Height);
}

void CorrectRenderTextureSizeForCubemap(ref int Width,ref int Height)
{
if (!Mathf.IsPowerOfTwo (Width))
Width = Mathf.NextPowerOfTwo (Width);
if (!Mathf.IsPowerOfTwo (Height))
Height = Mathf.NextPowerOfTwo (Height);

//	must be pow2 and same width/height
Width = Mathf.Max( Width, Height );
Height = Width;
}


void RenderAndWrite(Camera Cam)
{
if (Cam == null)
return;
if (Cast == null)
return;

//	if no texture provided, make one
int Width = Cam.pixelWidth;
int Height = Cam.pixelHeight;

bool Success = false;

if ( CaptureMode == PopCaptureMode.Camera )
{
//	mp4 needs this to be certain sizes
CorrectRenderTextureSizeForMp4 (ref Width, ref Height);
if (CaptureCameraTexture == null)
CaptureCameraTexture = new RenderTexture( Width, Height, 16 );

//	save settings to restore
RenderTexture PreTarget = RenderTexture.active;
var PreCameraTarget = Cam.targetTexture;

Cam.targetTexture = CaptureCameraTexture;
RenderTexture.active = CaptureCameraTexture;
Cam.Render();

//	aaaand restore.
RenderTexture.active = PreTarget;
Cam.targetTexture = PreCameraTarget;

Success = true;
}
else if ( CaptureMode == PopCaptureMode.Cubemap )
{
//	mp4 needs this to be certain sizes
CorrectRenderTextureSizeForMp4 (ref Width, ref Height);
CorrectRenderTextureSizeForCubemap (ref Width, ref Height);
if (CaptureCameraTexture == null)
{
	CaptureCameraTexture = new RenderTexture( Width, Height, 16 );
	CaptureCameraTexture.isCubemap = true;
}

int FaceMask = 63;
Success = Cam.RenderToCubemap( CaptureCameraTexture, FaceMask );
}

if ( Success )
Cast.UpdateTexture( CaptureCameraTexture, 0);
}

	void LateUpdate()
	{
		//	capture camera
		if (CaptureCamera != null)
			RenderAndWrite (CaptureCamera);
		else if (Camera.main != null)
			RenderAndWrite (Camera.main);
		


	}

	void Update()
	{

	}
}
