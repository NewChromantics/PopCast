using UnityEngine;
using System.Collections;
using System.Collections.Generic;


public class WindowEnum 
{
	public RenderTexture			Texture; 
	public string					Source;
	public WindowSourceGuiThumbnail	ThumbnailGui;
};



public class WindowEnumerator : MonoBehaviour {

	public WindowSourceGuiThumbnail	TemplateThumbnail;
	public int						MaxWindows = 3;
	private uint					EnumIndex = 0;
	public List<WindowEnum>			Windows;
	public string					WindowPrefix = "window:";
	public string					WindowFilter;
	public Material					InitThumbnailShader;
	static Texture2D				ClearTextureCache;

	void ClearTexture(RenderTexture Tex)
	{
		if ( InitThumbnailShader != null )
		{
			Graphics.Blit( Tex, InitThumbnailShader );
			return;	
		}

		if (ClearTextureCache == null)
		{ 
			ClearTextureCache = new Texture2D(1,1);
			ClearTextureCache.SetPixel( 0, 0, new Color(0,0,0) );
		}

		Graphics.Blit( Tex, ClearTextureCache );
	}

	void FindNewSource()
	{ 
		string SourceName = PopMovie.EnumSource(EnumIndex);

		if ( SourceName == null )
			return;

		if (SourceName.StartsWith(WindowPrefix))
		{ 
			bool Filtered = (WindowFilter == null || WindowFilter.Length == 0) ? true : SourceName.Contains(WindowFilter);
			if ( Filtered )
			{
				 AddNewSource(SourceName);
			}
		}

		EnumIndex++;
	}

	void AddNewSource(string SourceName)
	{ 
		Debug.Log("New window " + SourceName );

		if ( TemplateThumbnail == null )
			return;

		if ( Windows != null && Windows.Count >= MaxWindows )
			return;

		WindowEnum Enum = new WindowEnum();
		Enum.Source = SourceName;
		Enum.Texture = new RenderTexture(128,128,0);
		ClearTexture( Enum.Texture );
		Enum.ThumbnailGui = Instantiate( TemplateThumbnail );
		Enum.ThumbnailGui.gameObject.SetActive( true );
		Enum.ThumbnailGui.transform.SetParent( TemplateThumbnail.transform.parent );
		Enum.ThumbnailGui.Init( Enum.Source, Enum.Texture );
		if ( Windows == null )
			Windows = new List<WindowEnum>();
		Windows.Add( Enum );
	}


	void Update () {
		FindNewSource();
	}
}
