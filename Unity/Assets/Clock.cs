using UnityEngine;
using System.Collections;

public class Clock : MonoBehaviour {

	public UnityEngine.UI.Text	mText;

	void Update () {

		if ( mText != null )
			mText.text = "" + Time.time;
	
	}
}
