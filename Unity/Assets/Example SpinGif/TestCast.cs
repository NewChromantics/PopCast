using UnityEngine;
using System.Collections;
using System.Collections.Generic;

public class TestCast : MonoBehaviour {

	public List<Texture>	OutputTextures;	//	one per stream

	public PopCastParams	CastParams;

	public PopCast		mCast = null;
	public string		mOutputTarget = "PopCast.mp4";

	public Camera			mCaptureCamera;
	public RenderTexture	mCaptureCameraTexture;

	void OnEnable()
	{
		if (mCast == null) {
			mCast = new PopCast (mOutputTarget, CastParams);
			mCast.AddDebugCallback (Debug.Log);
		}
	}

	void LateUpdate()
	{
		//	capture camera
		if (mCaptureCamera != null) {
			
			//	if no texture provided, make one
			if (mCaptureCameraTexture == null)
				mCaptureCameraTexture = new RenderTexture (mCaptureCamera.pixelWidth, mCaptureCamera.pixelHeight, 16);

			//	save settings to restore
			RenderTexture PreTarget = RenderTexture.active;
			var PreCameraTarget = mCaptureCamera.targetTexture;

			mCaptureCamera.targetTexture = mCaptureCameraTexture;
			RenderTexture.active = mCaptureCameraTexture;
			mCaptureCamera.Render ();

			//	aaaand restore.
			RenderTexture.active = PreTarget;
			mCaptureCamera.targetTexture = PreCameraTarget;
		}
		
	}

	void Update () {

		//	write latest stream data
		if (mCast != null) 
		{
			//	check where to output the camera, if we haven't already
			bool CameraCaptureOutput = false;
			int FirstStreamUnused = -1;

			//	output textures
			for ( int i=0;	i<OutputTextures.Count;	i++ )
			{
				var Tex = OutputTextures[i];
				if ( Tex == null )
				{
					if ( FirstStreamUnused == -1 )
						FirstStreamUnused = i;
					continue;
				}
				CameraCaptureOutput |= Tex == mCaptureCameraTexture;
				mCast.UpdateTexture( Tex, i );
			}

			//	if we want to write the capture texture, and we havent... do it now
			if ( !CameraCaptureOutput && mCaptureCameraTexture != null )
			{
				//	if we haven't left a gap... do it at the end 
				if ( FirstStreamUnused == -1 )
					FirstStreamUnused = OutputTextures.Count;
				mCast.UpdateTexture( mCaptureCameraTexture, FirstStreamUnused );
			}
		}
	
	}
}
