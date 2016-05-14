using UnityEngine;
using System.Collections;

[ExecuteInEditMode]
public class UpdatePopCastGui : MonoBehaviour {

	public PopCastCameraCapture mPopCast;
	public UnityEngine.UI.Slider mSlider;


	// Use this for initialization
	void Start () {
		if (mPopCast == null)
			mPopCast = this.GetComponent<PopCastCameraCapture>();
	
	}
	
	// Update is called once per frame
	void Update () {

		int PendingFrameCount = 0;
		if (mPopCast != null && mPopCast.mCast != null)
			PendingFrameCount = mPopCast.mCast.GetPendingFrameCount();

		if (mSlider != null)
		{
			if (mSlider.maxValue < PendingFrameCount)
				mSlider.maxValue = PendingFrameCount;
			mSlider.value = PendingFrameCount;
		}
	}
}
