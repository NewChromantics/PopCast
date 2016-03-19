using UnityEngine;
using System.Collections;
using UnityEditor;



public class PopCastWindow : EditorWindow
{
	[Header("Use full path, or StreamingAssets/ or DataPath/ Use a filename or *.gif to timestamp filename")]
	public string			Filename = "file:StreamingAssets/*.gif";

	public PopCastParams	Parameters;

	[Header("If no camera specified, the main camera will be recorded")]
	public Camera			RecordCamera = null;

	[Header("If no target texture set, one will be created")]
	public RenderTexture	RecordToTexture = null;


	private PopCast 		mPopCast;

	[MenuItem ("PopCast/Editor recorder")]

	public static void  ShowWindow () {
		EditorWindow.GetWindow(typeof(PopCastWindow));
	}

	void OnGUI ()
	{
		string DebugString = "";
		DebugString += "Version: " + PopCast.GetVersion() + "\n";
		DebugString += "Frames pushed: " + (mPopCast==null ? -1 : mPopCast.GetTexturePushCount()) + "\n";
		EditorGUILayout.HelpBox ( DebugString, MessageType.None );

		if (mPopCast != null) {
			if (GUILayout.Button ("Stop recording")) {
				StopRecording ();
			}
		} else {
			if (GUILayout.Button ("Start recording")) {
				StartRecording ();
			}
		}

		//	reflect built in properties
		var ThisEditor = Editor.CreateEditor(this);
		ThisEditor.OnInspectorGUI();

		if ( mPopCast != null )
		{
			Repaint();
		}
	}

	void StopRecording()
	{
		if ( mPopCast != null )
		{
			//	stop/dealloc immedaitely
			mPopCast.Stop();
			mPopCast = null;
		}
	}

	void StartRecording()
	{
		if ( RecordCamera == null )
		{
			RecordCamera = Camera.allCameras [0];
			Debug.Log( Camera.allCameras );
		}

		//	if no texture provided, make one
		if (RecordToTexture == null)
			RecordToTexture = new RenderTexture (RecordCamera.pixelWidth, RecordCamera.pixelHeight, 16);

		mPopCast = new PopCast (Filename, Parameters);
		mPopCast.AddDebugCallback (Debug.Log);

	}

	void LateUpdate()
	{
		//Debug.Log ("Popcast window LateUpdate");
		//	was in lateupdate
		//	update recording
		if ( mPopCast != null )
		{
			//	save settings to restore
			RenderTexture PreTarget = RenderTexture.active;
			var PreCameraTarget = RecordCamera.targetTexture;

			RecordCamera.targetTexture = RecordToTexture;
			RenderTexture.active = RecordToTexture;
			RecordCamera.Render ();

			//	aaaand restore.
			RenderTexture.active = PreTarget;
			RecordCamera.targetTexture = PreCameraTarget;
		}

	}

	void Update()
	{
		//Debug.Log ("Popcast window update");
		//	update texture
		LateUpdate ();

		//	write latest stream data
		if (mPopCast != null) 
		{
			mPopCast.UpdateTexture( RecordToTexture, 0 );
		}


	}
}
