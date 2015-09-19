using UnityEngine;
using System.Collections;

public class TestCast : MonoBehaviour {

	public float	Wait = 2;
	public PopCast	mCast = null;

	// Use this for initialization
	void Start () {
	
	}
	
	// Update is called once per frame
	void Update () {
		if (mCast != null) {
			mCast.Update();
			return;
		}

		Wait -= Time.deltaTime;
		if (Wait > 0)
			return;

		mCast = new PopCast ("chromecast:*", new PopCastParams ());


	}
}
