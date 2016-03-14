using UnityEngine;
using System.Collections;
using UnityEditor;


public class PopCastWindow : EditorWindow {

	[Header("Pop open finder/explorer when gif is ready")]
	public bool				ShowFileWhenFinished = true;
	//[Header("Limit filesize. Twitter's limit is 4mb (4096kb)")]
	//public uint				KilobyteLimit = 4 * 1024;

	[Header("If no camera specified, the editor camera will be recorded")]
	public Camera			RecordCamera = null;
	[Header("If target set, one will be created")]
	public RenderTexture	RecordToTexture = null;
	public string			RecordPath = "StreamingAssets:*.gif";
	private PopCast 		mPopCast;

	[MenuItem ("PopCast/Editor recorder")]

	public static void  ShowWindow () {
		EditorWindow.GetWindow(typeof(PopCastWindow));
	}

	void OnGUI ()
	{		
		/*
		myString = EditorGUILayout.TextField ("Text Field", myString);

		groupEnabled = EditorGUILayout.BeginToggleGroup ("Optional Settings", groupEnabled);
		myBool = EditorGUILayout.Toggle ("Toggle", myBool);
		myFloat = EditorGUILayout.Slider ("Slider", myFloat, -3, 3);
		EditorGUILayout.EndToggleGroup ();
		*/

		if (mPopCast != null) {
			if (GUILayout.Button ("Stop recording")) {
				StopRecording ();
			}
		} else {
			if (GUILayout.Button ("Start recording")) {
				StartRecording ();
			}
		}


		var editor = Editor.CreateEditor(this);
		editor.OnInspectorGUI();        
	}

	void StopRecording()
	{
		mPopCast = null;
		System.GC.Collect ();
	}

	void StartRecording()
	{
		if ( RecordCamera == null )
		{
			RecordCamera = Camera.allCameras [0];
		}

		//	if no texture provided, make one
		if (RecordToTexture == null)
			RecordToTexture = new RenderTexture (RecordCamera.pixelWidth, RecordCamera.pixelHeight, 16);
		
		mPopCast = new PopCast (RecordPath, new PopCastParams ());
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
