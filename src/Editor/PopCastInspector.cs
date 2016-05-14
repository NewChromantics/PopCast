using UnityEngine;
using System.Collections;
using UnityEditor;



// project, else this will not work.
[CustomEditor(typeof(PopCastCameraCapture))]
public class PopCastInspector : Editor
{
	static public void ApplyInspector(PopCast Instance, Object target, Editor ThisEditor)
	{
		GUILayout.Label("PopCast Version " + PopCast.GetVersion());
		PopCast.mAllowBackgroundProcessing = GUILayout.Toggle(PopCast.mAllowBackgroundProcessing, "Allow background jobs");

		string MetaString = (Instance != null) ? Instance.GetMetaJson() : null;
		if (MetaString == null) 
		{
			if (Instance != null) 
			{
				MetaString = "<no meta>";
			} 
			else if (Instance == null) 
			{
				MetaString = "<no instance>";
			}
		}


		//	pop cast has some GPU processing to do, and if the editor is paused or stopped,
		//	we'll be waiting for renderthread events, so we need to force them to finish encodings
		var BackgroundJobs = PopCast.GetBackgroundGpuJobCount();

		if (BackgroundJobs > 0)
		{
			string Message = "" + BackgroundJobs + " background jobs";

			if (!EditorApplication.isUpdating)
			{
				ThisEditor.Repaint();
				if (PopCast.mAllowBackgroundProcessing)
				{
					Message += " (forcing GPU update)";
					PopCast.Update();
				}
			}

			Message += "\n" + MetaString;

			EditorGUILayout.HelpBox(Message, MessageType.Warning, true);
		}
		else
		{
			string Message = "Idle (no background jobs)";
			Message += "\n" + MetaString;
			EditorGUILayout.HelpBox(Message, MessageType.Info, true);
		}

		if (Instance != null)
			ThisEditor.Repaint();
	}


	public override void OnInspectorGUI()
	{
		var This = target as PopCastCameraCapture;
		ApplyInspector(This.mCast, target, this);

		DrawDefaultInspector();
	}
}

