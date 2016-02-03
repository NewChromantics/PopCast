REM Assuming we have git installed, we can just use the shell scripts!

@echo Windows post build

REM Set up env in the batch file, copy & paste this.
REM set SRCROOT=$(ProjectDir)\..\
REM set PROJECT=PopMovieTexture
REM set UNITY_EXE=$(UnityExe)
REM set BUILD_DLL=$(TargetPath)
 
REM Setup some other vars we don't need to put in VS
set TARGET_PATH=%SRCROOT%\Unity\Assets\Plugins\Windows\
set TARGET_WINDOWS=true

REM todo: check these vars are set
REM echo SRCROOT=%SRCROOT%
REM echo PROJECT=%PROJECT%
REM echo UNITY_EXE=%UNITY_EXE%
REM echo BUILD_DLL=%BUILD_DLL%
REM echo TARGET_PATH=%TARGET_PATH%
if not defined SRCROOT			(	echo SRCROOT env var not defined.		& exit /b 1001	)
if not defined PROJECT			(	echo PROJECT env var not defined.		& exit /b 1002	)
if not defined UNITY_EXE		(	echo UNITY_EXE env var not defined.		& exit /b 1003	)
if not defined BUILD_DLL		(	echo BUILD_DLL env var not defined.		& exit /b 1004	)
if not defined TARGET_PATH		(	echo TARGET_PATH env var not defined.	& exit /b 1005	)

if not exist "%BUILD_DLL%"		(	echo Output binary doesn't exist; %BUILD_DLL%.	& exit /b 1006	)

REM Copy binaries
echo on
if not exist "%TARGET_PATH%" (
	mkdir "%TARGET_PATH%"
	if %ERRORLEVEL% NEQ 0	(	EXIT /b 1007	)
)

echo "Copying %BUILD_DLL% to Unity dir %TARGET_PATH%"
copy /Y "%BUILD_DLL%" "%TARGET_PATH%"
if %ERRORLEVEL% NEQ 0	(	EXIT /b 1008	)

REM for windows, in case the scripts below don't work, force an early C# copy
set BUILD_CSHARP=%SRCROOT%\src\%PROJECT%.cs
set TARGET_CSHARP_PATH=%TARGET_PATH%"..\
echo "Copying %BUILD_CSHARP% to Unity dir %TARGET_CSHARP_PATH%"
copy /Y "%BUILD_CSHARP%" "%TARGET_CSHARP_PATH%"
if %ERRORLEVEL% NEQ 0	(	EXIT /b 1008	)


set BASH_FILENAME=bash.exe

REM this doesn't seem to work inside the if() ???
set BASH_SEARCH_PATH=%LOCALAPPDATA%\github

if not defined BASH_EXE (

	REM github installs bash in a temp guid dir in local data (win7)
	echo searching %BASH_SEARCH_PATH%

	REM gr target filename variable MUST be single character!
	for /R %BASH_SEARCH_PATH% %%F in (%BASH_FILENAME%) do (
		
		if exist %%F (
			set BASH_EXE=%%F
			REM echo found bash at %%F
			goto find_bash_break
		) else ( 
			REM echo found file that doesn't exist... %%F
		)
	)

	:find_bash_break
	REM no blank lines after label.
)

if not defined BASH_EXE (
	echo Failed to find bash.exe, cannot run post build scripts.
	exit /b 1012
)
echo using bash at %BASH_EXE%

REM Normal post build
%BASH_EXE% %SRCROOT%/CopyCSharpFiles.sh
if %ERRORLEVEL% NEQ 0	(
	echo Warning: makepackage.sh failed
	REM exit /b 1009	
)

%BASH_EXE% %SRCROOT%/makepackage.sh
if %ERRORLEVEL% NEQ 0	(
	echo Warning: makepackage.sh failed
	REM exit /b 1010	
)

REM success
exit 0

