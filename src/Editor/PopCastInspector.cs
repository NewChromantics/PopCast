using UnityEngine;
using System.Collections;
using UnityEditor;



// project, else this will not work.
[CustomEditor(typeof(PopCastCameraCapture))]
public class PopCastInspector : Editor
{
	static bool AllowBackgroundJobs = true;

	static public void ApplyInspector(PopCast Instance, Object target)
	{
		GUILayout.Label("PopCast Version " + PopCast.GetVersion());
		AllowBackgroundJobs = GUILayout.Toggle(AllowBackgroundJobs, "Allow background jobs");

		string MetaString = "";
		var Meta = (Instance != null) ? Instance.GetMeta() : null;
		if (Meta != null)
		{
			MetaString += "BackgroundGpuJobCount: " + Meta.BackgroundGpuJobCount + "\n";
			MetaString += "InstanceCount: " + Meta.InstanceCount + "\n";
			MetaString += "MuxerInputQueueCount: " + Meta.MuxerInputQueueCount + "\n";
			MetaString += "MuxerDefferedQueueCount: " + Meta.MuxerDefferedQueueCount + "\n";
			MetaString += "BytesWritten: " + Meta.BytesWritten + "\n";
			MetaString += "PendingWrites: " + Meta.PendingWrites + "\n";
			MetaString += "PendingEncodedFrames: " + Meta.PendingEncodedFrames + "\n";
			MetaString += "PushedFrameCount: " + Meta.PushedFrameCount + "\n";
			MetaString += "PendingFrameCount: " + Meta.PendingFrameCount + "\n";
		}
		else if (Instance != null)
		{
			MetaString = "<no meta>";
		}
		else
		{
			MetaString = "<no instance>";
		}


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

			Message += "\n" + MetaString;

			EditorGUILayout.HelpBox(Message, MessageType.Warning, true);
		}
		else
		{
			string Message = "Idle (no background jobs)";
			Message += "\n" + MetaString;
			EditorGUILayout.HelpBox(Message, MessageType.Info, true);
		}

		if ( Instance != null )
			EditorUtility.SetDirty(target);
	}


	public override void OnInspectorGUI()
	{
		var This = target as PopCastCameraCapture;
		ApplyInspector(This.mCast, target);

		DrawDefaultInspector();
	}
}

