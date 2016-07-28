/*

	Unity causes large stalls when reading the GetNativePtr from textures as it requires a block to the thread.
	Not cool, so caching it ourselves

*/
using UnityEngine;
using System.Collections;					// required for Coroutines
using System.Runtime.InteropServices;		// required for DllImport
using System;								// requred for IntPtr
using System.Text;
using System.Collections.Generic;


public class PopTexturePtrCache<TEXTURE> where TEXTURE : Texture
{
	public TEXTURE			mTexture;
	public System.IntPtr	mPtr;
};

public class PopTexturePtrCache
{
	//	cache the texture ptr's. Unity docs say accessing them causes a GPU sync, I don't believe they do, (edit: definitely does in VR mode) BUT we want to avoid setting the active render texture anyway
	static Dictionary<Texture,System.IntPtr>	mCache;

	static Dictionary<Texture,System.IntPtr>	GetCache()
	{
		if (mCache == null)
			mCache = new Dictionary<Texture,System.IntPtr> ();
		return mCache;
	}

	static public System.IntPtr GetNativeTexturePtr<T>(T texture) where T : Texture
	{
		var Cache = GetCache ();
		if ( !Cache.ContainsKey( texture ) )
			Cache [texture] = texture.GetNativeTexturePtr ();
		return Cache [texture];
	}

	//	render texture overload
	static public System.IntPtr GetNativeTexturePtr(RenderTexture texture)
	{
		var Cache = GetCache ();
		if ( !Cache.ContainsKey( texture ) )
		{
			var Prev = RenderTexture.active;
			RenderTexture.active = texture;
			Cache [texture] = texture.GetNativeTexturePtr ();
			RenderTexture.active = Prev;
		}
		return Cache [texture];
	}
};
