using UnityEngine;
using System.Collections;

public class TestCast : MonoBehaviour {

	public float		Wait = 2;
	public PopCast		mCast = null;
	public string		mOutputTarget = "chromecast:*";
	public Texture2D	mOutputTexture;

	// Use this for initialization
	void Start () {
	
	}
	
	// Update is called once per frame
	void Update () {
		if (mCast != null) {
			mCast.UpdateTexture(mOutputTexture);
			return;
		}

		Wait -= Time.deltaTime;
		if (Wait > 0) {
			Debug.Log ("Waiting... " + Wait);
			return;
		}

		mCast = new PopCast (mOutputTarget, new PopCastParams ());
		mCast.AddDebugCallback (Debug.Log);

	}
}
