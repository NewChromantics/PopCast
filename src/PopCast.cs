using UnityEngine;
using System.Collections;					// required for Coroutines
using System.Runtime.InteropServices;		// required for DllImport
using System;								// requred for IntPtr


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
	private static extern ulong		PopCast_Alloc(PopCastParams Params);
	
	[DllImport (PluginName)]
	private static extern bool		PopCast_Free(ulong Instance);

	[DllImport (PluginName)]
	private static extern void		PopCast_EnumDevices();

	[DllImport (PluginName)]
	private static extern int		PopCast_GetPluginEventId();
	
	[DllImport (PluginName)]
	private static extern bool		FlushDebug([MarshalAs(UnmanagedType.FunctionPtr)]System.IntPtr FunctionPtr);


	public static void EnumDevices()
	{
		PopCast_EnumDevices();
	}

	public PopCast(string Filename,PopCastParams Params)
	{
		mInstance = PopCast_Alloc ( Params );

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
	
	
	public void Update()
	{
		GL.IssuePluginEvent (mPluginEventId);
		FlushDebug();
	}


}

