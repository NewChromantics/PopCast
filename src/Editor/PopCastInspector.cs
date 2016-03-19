using UnityEngine;
using System.Collections;
using UnityEditor;



// project, else this will not work.
[CustomEditor(typeof(PopCastCameraCapture))]
public class PopCastInspector : Editor
{
	static bool AllowBackgroundJobs = true;

	static public void ApplyInspector(Editor Inspector, Object target)
	{
		GUILayout.Label("PopCast Version " + PopCast.GetVersion());
		AllowBackgroundJobs = GUILayout.Toggle(AllowBackgroundJobs, "Allow background jobs");

		//	pop cast has some GPU processing to do, and if the editor is paused or stopped,
		//	we'll be waiting for renderthread events, so we need to force them to finish encodings
		var BackgroundJobs = PopCast.GetBackgroundGpuJobCount();

		if (BackgroundJobs > 0)
		{
			string Message = "" + BackgroundJobs + " background jobs";

			if (!EditorApplication.isUpdating)
			{
				EditorUtility.SetDirty(target);
				if (AllowBackgroundJobs)
				{
					Message += " (forcing GPU update)";
					PopCast.Update();
				}
			}
			EditorGUILayout.HelpBox(Message, MessageType.Warning, true);
		}
		else
		{
			EditorGUILayout.HelpBox("Idle", MessageType.Info, true);
		}

	}


	public override void OnInspectorGUI()
	{
		ApplyInspector(this, target);

		DrawDefaultInspector();
	}
}

