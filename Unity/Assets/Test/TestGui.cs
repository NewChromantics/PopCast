using UnityEngine;
using System.Collections;
using System.Collections.Generic;

public class TestGui : MonoBehaviour {

	public static List<string>	mLog = new List<string>();
	[Range(1,100)]
	public int			mFontSize = 20;
	public int 			mMaxLines = 40;
	public int			mMaxLineWidth = 40;


	
	public static void Log(string debug)
	{
		mLog.Insert (0,debug);
	}

	void Start()
	{
	}

	void Update()
	{
		PopCast.FlushDebug( Log );
	}

	void OnGUI() {

		int Border = 10;

		Rect rect = new Rect (Border, Border, Screen.width - Border * 2, 20);

		//	list button
		if (GUI.Button (rect, "List devices")) {
			PopCast.EnumDevices ();
		}


		rect.y += rect.height;
		rect.height = Screen.height - rect.y;

		GUIStyle Style = new GUIStyle ();
		Style.fontSize = mFontSize;
		
		if ( mLog.Count > mMaxLines )
			mLog.RemoveRange (mMaxLines, mLog.Count - mMaxLines);
		
		string Log = "";
		foreach (string _line in mLog) {
			
			string line = _line;
			while (line.Length > mMaxLineWidth) {
				Log += line.Substring (0, mMaxLineWidth) + "\n";
				line = line.Remove (0, mMaxLineWidth);
			}
			Log += line + "\n";
		}
		GUI.Label ( rect, Log, Style);
		
	}
}
