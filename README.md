PopCast
=============================================

In-game & in-editor recorder for unity
---------------------------------------------
+ [popcast.xyz](http://www.popcast.xyz)

Now Open Source
---------------------------------------------
+ This was previously a closed source project, but due to my limited time, development has slowed too much, so now it is open source
+ New releases, demos etc will appear here over time, but the current watermarked builds are at [the old release repository](https://github.com/NewChromantics/PopCast_Release/releases)
+ Pull requests are welcome.

Build instructions
===========================
+ Note, these are taken from PopMovie, but they're basically the same.

Windows Setup
---------------------------------------------
- Download visual studio 2013 community edition.
  - vs2015 can be used, toolset is currently set to v120. Can change this in the project settings.
- Download PopMovieTexture & SoyLib repos
- Open property manager and open `Microsoft.Cpp.x64.user` and in user macros add
  - name `SOY_PATH` value `c:\soylib` (or wherever your root of soylib is). doesn't need to be an env var
  - name `UNITY_EXE` value `C:\Program Files\Unity5.3.6f1\Editor\unity.exe` (or wherever unity.exe is). Doesn't need to be an env var
- Repeat for `Microsoft.Cpp.win32.user`
- May need to reload project/solution for it to reload the soy props.
- For VS2015, change toolset (compile will fail before this if you don't have VS2013 installed)
- Compile!
- To debug, in general properties for PopMovieTexture, set command to `$(UNITY_EXE)`

OSX setup
---------------------------------------------
- Download latest xcode
- Add OSX10.10 sdk (required by unity) from here; https://github.com/phracker/MacOSX-SDKs
- Install [Home brew](http://brew.sh/) `/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"`
- Setup xcode source tree paths; (print out current setup with `defaults read com.apple.dt.xcode`)
  - `set SOYLIB_PATH=<your path to soylib repos>`
  - `set UNITY_APP_PATH=/Volumes/Applications/Unity`
  - `defaults write com.apple.dt.xcode IDEApplicationwideBuildSettings -dict-add SOY_PATH %SOYLIB_PATH%`
  - `defaults write com.apple.dt.xcode IDEApplicationwideBuildSettings -dict-add UNITY_APP_PATH %UNITY_APP_PATH%`
- Alternatively These source tree paths can be set manually in `XCode -> preferences -> source trees`
- Restart xcode if it was open (These work like env vars so process needs to reset and projects reload)
- Compile!

IOS setup
---------------------------------------------
- Same as XCode, but doesn't require libusb, libfreenect or the osx10.10 sdk


Android Setup with xcode/osx
---------------------------------------------
- Download Xcode
- Install homebrew (as in OSX guide)
- Install android SDK & NDK
  - `brew install android-sdk`
  - `brew install android-ndk`
- Setup env vars for make (gr: I can't remember if these can be done as source trees, but teamcity is setup as env vars before the build). So these need to be places in  `~/.bash_profile` or somewhere similar. The android compile shell script should error if they're missing..
  - `set ANDROID_NDK=/usr/local/Cellar/android-ndk/r11c/`
  - `set ANDROID_SDK=/usr/local/Cellar/android-sdk/24.4.1_1` (Not required, but useful)
- Compile with android target
