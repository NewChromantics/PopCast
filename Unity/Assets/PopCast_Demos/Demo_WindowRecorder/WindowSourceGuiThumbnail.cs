using UnityEngine;
using System.Collections;

public class WindowSourceGuiThumbnail : MonoBehaviour {

	public PopMovieParams	Params;
	PopMovie		Movie;
	Texture			ThumbnailTexture;
	float			MovieTime = 0;

	public void Init(string Filename,Texture _ThumbnailTexture)
	{
		ThumbnailTexture = _ThumbnailTexture;

		var ThumbnailImage = GetComponent<UnityEngine.UI.RawImage>();
		if (ThumbnailImage != null)
		{ 
			ThumbnailImage.texture = ThumbnailTexture;
		}
		
		var Label = GetComponent<UnityEngine.UI.Text>();
		if (Label != null)
		{ 
			Label.text = Filename;
		}

		try
		{
			Movie = new PopMovie(Filename, Params);
		}
		catch (System.Exception e)
		{
			Debug.LogWarning("Failed to create popmovie window capture: "  + Filename ); 
		}

	}

	public void Update()
	{
		MovieTime += Time.deltaTime;
		if (Movie != null && ThumbnailTexture != null)
		{
			Movie.SetTime( MovieTime );
			Movie.UpdateTexture( ThumbnailTexture );
		}

	}


}
