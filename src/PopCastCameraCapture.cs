using UnityEngine;
using System.Collections;
using System.Collections.Generic;

public class PopCastCameraCapture : MonoBehaviour
{

	public PopCastParams CastParams;
	public PopCast mCast = null;
	public string mOutputTarget = "file:StreamingAssets/*.gif";
	public Camera mCaptureCamera;
	public RenderTexture mCaptureCameraTexture;
	public bool EnableDebug = true;

	void OnEnable()
	{
		if (mCast == null)
		{
			mCast = new PopCast(mOutputTarget, CastParams);
			if (EnableDebug)
				PopCast.EnableDebugLog = EnableDebug;
		}
	}

	void OnDisable()
	{
		if ( mCast != null )
		{
			mCast.Free();
			mCast = null;
		}
	}

	void RenderAndWrite(Camera Cam)
	{
		if (Cam == null)
			return;
		if (mCast == null)
			return;

		//	if no texture provided, make one
		if (mCaptureCameraTexture == null)
			mCaptureCameraTexture = new RenderTexture(Cam.pixelWidth, Cam.pixelHeight, 16);
		
		//	save settings to restore
		RenderTexture PreTarget = RenderTexture.active;
		var PreCameraTarget = Cam.targetTexture;
		
		Cam.targetTexture = mCaptureCameraTexture;
		RenderTexture.active = mCaptureCameraTexture;
		Cam.Render();
		
		//	aaaand restore.
		RenderTexture.active = PreTarget;
		Cam.targetTexture = PreCameraTarget;
	
		mCast.UpdateTexture(mCaptureCameraTexture, 0);
	}


	void LateUpdate()
	{
		//	capture camera
		if (mCaptureCamera != null)
			RenderAndWrite (mCaptureCamera);
		else if (Camera.main != null)
			RenderAndWrite (Camera.main);
		


	}

	void Update()
	{

	}
}
