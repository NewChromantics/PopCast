using UnityEngine;
using System.Collections;					// required for Coroutines
using System.Runtime.InteropServices;		// required for DllImport
using System;								// requred for IntPtr




public class TTexturePtrCache<TEXTURE> where TEXTURE : Texture
{
public TEXTURE			mTexture;
public System.IntPtr	mPtr;
};

public class TexturePtrCache
{
static public System.IntPtr GetCache<T>(ref TTexturePtrCache<T> Cache,T texture) where T : Texture
{
if (Cache==null)
return texture.GetNativeTexturePtr();

if ( texture.Equals(Cache.mTexture) )
return Cache.mPtr;
Cache.mPtr = texture.GetNativeTexturePtr();
if ( Cache.mPtr != System.IntPtr.Zero )
Cache.mTexture = texture;
return Cache.mPtr;
}

static public System.IntPtr GetCache(ref TTexturePtrCache<RenderTexture> Cache,RenderTexture texture)
{
if (Cache==null)
return texture.GetNativeTexturePtr();

if ( texture.Equals(Cache.mTexture) )
return Cache.mPtr;
var Prev = RenderTexture.active;
RenderTexture.active = texture;
Cache.mPtr = texture.GetNativeTexturePtr();
RenderTexture.active = Prev;
if ( Cache.mPtr != System.IntPtr.Zero )
Cache.mTexture = texture;
return Cache.mPtr;
}
};



[StructLayout(LayoutKind.Sequential),Serializable]
public class  PopCastParams
{
	public bool				mTest = true;
}

public class PopCast
{
#if UNITY_STANDALONE_OSX || UNITY_EDITOR_OSX
	private const string PluginName = "PopCastOsx";
#elif UNITY_ANDROID
	private const string PluginName = "PopCast";
#elif UNITY_IOS
	//private const string PluginName = "PopCastIos";
	private const string PluginName = "__Internal";
#elif UNITY_STANDALONE_WIN || UNITY_EDITOR_WIN
	private const string PluginName = "PopCast";
#endif

	private ulong	mInstance = 0;
	private static int	mPluginEventId = PopCast_GetPluginEventId();

	//	cache the texture ptr's. Unity docs say accessing them causes a GPU sync, I don't believe they do, BUT we want to avoid setting the active render texture anyway
	private TTexturePtrCache<Texture2D>		mTexture2DPtrCache = new TTexturePtrCache<Texture2D>();
	private TTexturePtrCache<RenderTexture>	mRenderTexturePtrCache = new TTexturePtrCache<RenderTexture>();

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	public delegate void DebugLogDelegate(string str);
	private DebugLogDelegate	mDebugLogDelegate = null;
	
	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	public delegate void OpenglCallbackDelegate();

	public void AddDebugCallback(DebugLogDelegate Function)
	{
		if ( mDebugLogDelegate == null ) {
			mDebugLogDelegate = new DebugLogDelegate (Function);
		} else {
			mDebugLogDelegate += Function;
		}
	}
	
	public void RemoveDebugCallback(DebugLogDelegate Function)
	{
		if ( mDebugLogDelegate != null ) {
			mDebugLogDelegate -= Function;
		}
	}
	
	void DebugLog(string Message)
	{
		if ( mDebugLogDelegate != null )
			mDebugLogDelegate (Message);
	}

	public bool IsAllocated()
	{
		return mInstance != 0;
	}
	
	[DllImport (PluginName, CallingConvention = CallingConvention.Cdecl)]
	private static extern ulong		PopCast_Alloc(String Filename);
	
	[DllImport (PluginName)]
	private static extern bool		PopCast_Free(ulong Instance);

	[DllImport (PluginName)]
	private static extern void		PopCast_EnumDevices();

	[DllImport (PluginName)]
	private static extern int		PopCast_GetPluginEventId();
	
	[DllImport (PluginName)]
	private static extern bool		FlushDebug([MarshalAs(UnmanagedType.FunctionPtr)]System.IntPtr FunctionPtr);

	[DllImport (PluginName)]
	private static extern bool		PopCast_UpdateTexture2D(ulong Instance,System.IntPtr TextureId,int Width,int Height,TextureFormat textureFormat,int StreamIndex);

	[DllImport (PluginName)]
	private static extern bool		PopCast_UpdateRenderTexture(ulong Instance,System.IntPtr TextureId,int Width,int Height,RenderTextureFormat textureFormat,int StreamIndex);


	public static void EnumDevices()
	{
		PopCast_EnumDevices();
	}

	public PopCast(string Filename,PopCastParams Params)
	{
		mInstance = PopCast_Alloc ( Filename );

		//	if this fails, capture the flush and throw an exception
		if (mInstance == 0) {
			string AllocError = "";
			FlushDebug (
				(string Error) => {
				AllocError += Error; }
			);
			if ( AllocError.Length == 0 )
				AllocError = "No error detected";
			throw new System.Exception("PopCast failed: " + AllocError);
		}
	}
	
	~PopCast()
	{
		//	gr: don't quite get the destruction order here, but need to remove the [external] delegates in destructor. 
		//	Assuming external delegate has been deleted, and this garbage collection (despite being explicitly called) 
		//	is still deffered until after parent object[monobehaviour] has been destroyed (and external function no longer exists)
		mDebugLogDelegate = null;
		PopCast_Free (mInstance);
		FlushDebug ();
	}
	
	void FlushDebug()
	{
		FlushDebug (mDebugLogDelegate);
	}

	public static void FlushDebug(DebugLogDelegate Callback)
	{
		//	if we have no listeners, do fast flush
		bool HasListeners = (Callback != null) && (Callback.GetInvocationList ().Length > 0);
		if (HasListeners) {
			//	IOS (and aot-only platforms cannot get a function pointer. Find a workaround!
#if UNITY_IOS && !UNITY_EDITOR
			FlushDebug (System.IntPtr.Zero);
#else
			FlushDebug (Marshal.GetFunctionPointerForDelegate (Callback));
#endif
		} else {
			FlushDebug (System.IntPtr.Zero);
		}
	}

	public void UpdateTexture(Texture Target,int StreamIndex)
	{
		Update ();

		if (Target is RenderTexture) {
			RenderTexture Target_rt = Target as RenderTexture;
			PopCast_UpdateRenderTexture (mInstance, TexturePtrCache.GetCache (ref mRenderTexturePtrCache, Target_rt), Target.width, Target.height, Target_rt.format, StreamIndex);
		}
		if (Target is Texture2D) {
			Texture2D Target_2d = Target as Texture2D;
			PopCast_UpdateTexture2D (mInstance, TexturePtrCache.GetCache (ref mTexture2DPtrCache, Target_2d), Target.width, Target.height, Target_2d.format, StreamIndex);
		}
		FlushDebug ();
	}

	private void Update()
	{
		GL.IssuePluginEvent (mPluginEventId);
		FlushDebug();
	}


}

