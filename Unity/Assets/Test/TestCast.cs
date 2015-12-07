using UnityEngine;
using System.Collections;
using System.Collections.Generic;

public class TestCast : MonoBehaviour {

	public List<RenderTexture>	OutputRenderTextures;	//	one per stream
	public List<Texture2D>		OutputTextures;	//	one per stream

	public PopCast		mCast = null;
	public string		mOutputTarget = "PopCast.mp4";
	
	void OnEnable()
	{
		if (mCast == null) {
			mCast = new PopCast (mOutputTarget, new PopCastParams ());
			mCast.AddDebugCallback (Debug.Log);
		}
	}

	bool IsStreamIndexUsed(int StreamIndex)
	{
		if (StreamIndex < OutputRenderTextures.Count)
		if (OutputRenderTextures [StreamIndex] != null)
			return true;
		
		if (StreamIndex < OutputTextures.Count)
		if (OutputTextures [StreamIndex] != null)
			return true;

		return false;
	}

	int GetStreamIndex(int StreamIndex,List<int> UsedStreams)
	{
		//	calculate an index we can use
		var OriginalStreamIndex = StreamIndex;
		while (UsedStreams.Exists ( Index => Index == StreamIndex )) 
		{
			//	look for an alternative
			if (IsStreamIndexUsed (StreamIndex)) {
				StreamIndex++;
				continue;
			}
		}

		//	warn if this stream has already been used
		if (StreamIndex != OriginalStreamIndex)
			Debug.LogWarning ("Stream index " + OriginalStreamIndex + " is already used, switched to " + StreamIndex);
		   
		UsedStreams.Add (StreamIndex);
		return StreamIndex;
	}
	
	void Update () {


		//	write latest stream data
		if (mCast != null) 
		{
			//	cope with badly configured components by working out stream indexes at runtime if we need to
			List<int> UsedStreams = new List<int>();

			for ( int i=0;	i<OutputRenderTextures.Count;	i++ )
			{
				var Texture = OutputRenderTextures[i];
				if ( Texture == null )
					continue;
				var StreamIndex = GetStreamIndex( i, UsedStreams );
				mCast.UpdateTexture( Texture, StreamIndex );
			}

			for ( int i=0;	i<OutputTextures.Count;	i++ )
			{
				var Texture = OutputTextures[i];
				if ( Texture == null )
					continue;
				var StreamIndex = GetStreamIndex( i, UsedStreams );
				mCast.UpdateTexture( Texture, StreamIndex );
			}


		}
	
	}
}
