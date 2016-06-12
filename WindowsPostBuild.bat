REM Assuming we have git installed, we can just use the shell scripts!

@echo Windows post build
echo on

REM Set up env in the batch file, copy & paste this.
REM set SRCROOT=$(ProjectDir)\..\
REM set PROJECT=PopMovieTexture
REM set UNITY_EXE=$(UnityExe)
REM set BUILD_DLL=$(TargetPath)
REM SET ASSETS_PATH=Windows/$(Platform)/

REM Setup some other vars we don't need to put in VS
set PROJECT_ASSETS_PATH=%SRCROOT%\Unity\Assets\%PROJECT%
set TARGET_PATH=%PROJECT_ASSETS_PATH%\%ASSETS_PATH%
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
if not defined ASSETS_PATH		(	echo ASSETS_PATH env var not defined.	& exit /b 1005	)

if not exist "%BUILD_DLL%"		(	echo Output binary doesn't exist; %BUILD_DLL%.	& exit /b 1006	)


CALL :CopyBinaries
if %ERRORLEVEL% NEQ 0	(	EXIT /b 9990	)

CALL :CopyFilesSimple
if %ERRORLEVEL% NEQ 0	(	EXIT /b 9991	)

REM CALL :MakePackage
REM if %ERRORLEVEL% NEQ 0	(	EXIT /b 9992	)

CALL :MakePackageBat
if %ERRORLEVEL% NEQ 0	(	EXIT /b 9993	)


REM success
exit 0





REM Copy binaries
:CopyBinaries
if not exist "%TARGET_PATH%" (
	mkdir "%TARGET_PATH%"
	if %ERRORLEVEL% NEQ 0	(	EXIT /b 1007	)
)

echo "Copying %BUILD_DLL% to Unity dir... %TARGET_PATH%"
copy /Y "%BUILD_DLL%" "%TARGET_PATH%"
if %ERRORLEVEL% NEQ 0	(	EXIT /b 1008	)
EXIT /B 0





REM Copy files simple
:CopyFilesSimple

REM for windows, in case the scripts below don't work, force an early C# copy
set BUILD_CSHARP=%SRCROOT%\src\*.cs
set TARGET_CSHARP_PATH=%PROJECT_ASSETS_PATH%\
echo "Copying %BUILD_CSHARP% to Unity dir %TARGET_CSHARP_PATH%"
copy /Y "%BUILD_CSHARP%" "%TARGET_CSHARP_PATH%"
if %ERRORLEVEL% NEQ 0	(	EXIT /b 1013	)

set BUILD_CSHARPEDITOR=%SRCROOT%\src\Editor
set TARGET_CSHARPEDITOR_PATH=%PROJECT_ASSETS_PATH%\Editor\*
echo "Copying %BUILD_CSHARPEDITOR% to Unity dir %TARGET_CSHARPEDITOR_PATH%"
xcopy /E /Y "%BUILD_CSHARPEDITOR%" "%TARGET_CSHARPEDITOR_PATH%"
if %ERRORLEVEL% NEQ 0	(	EXIT /b 1014	)
EXIT /B 0



REM windows version which doesn't require bash
:MakePackageBat

REM disable with an env var
if defined DISABLE_MAKEPACKAGE (
	if %DISABLE_MAKEPACKAGE% NEQ 0	(
		echo DISABLE_MAKEPACKAGE is defined, skipping
		EXIT /B 0
	)
)

call %SRCROOT%/MakePackage.bat
if %ERRORLEVEL% NEQ 0	(
	echo Warning: makepackage.bat failed
	REM exit /b 1015	
)
EXIT /B 0





REM run normal scripts
:MakePackage

REM disable with an env var
if defined DISABLE_MAKEPACKAGE (
	if %DISABLE_MAKEPACKAGE% NEQ 0	(
		echo DISABLE_MAKEPACKAGE is defined, skipping
		EXIT /B 0
	)
)

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

REM extract path from bash exe and add to the env path, so calls to mkdir, cp etc work as they're alongside bash.exe
for %%a in (%BASH_EXE%) do (
	set BASH_PATH=%%~dpa
)
echo Adding %BASH_PATH% to path env
set PATH=%PATH%;%BASH_PATH%


REM Normal post build
%BASH_EXE% %SRCROOT%/CopyCSharpFiles.sh
if %ERRORLEVEL% NEQ 0	(
	echo Warning: CopyCSharpFiles.sh failed
	REM exit /b 1009	
)

%BASH_EXE% %SRCROOT%/makepackage.sh
if %ERRORLEVEL% NEQ 0	(
	echo Warning: makepackage.sh failed
	REM exit /b 1010	
)
EXIT /B 0

