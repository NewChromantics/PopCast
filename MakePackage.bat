

for /f "delims=" %%i in ('git rev-parse --verify --short HEAD') do set GIT_REV=%%i
echo GIT_REV=%GIT_REV%

REM set PACKAGE_FILENAME="%SRCROOT%/%PROJECT%.%GIT_REV%.unitypackage"
set PACKAGE_FILENAME="%SRCROOT%/%PROJECT%.unitypackage"
set PROJECT_PATH="%SRCROOT%/Unity"


set LOG_FILE="./MakePackage.log"
set PROJECT_PACKAGE_PATHS="Assets/%PROJECT%"

echo PROJECT_PATH=%PROJECT_PATH%
echo LOG_FILE=%LOG_FILE%

REM clean log file first
if exist %LOG_FILE% (
	echo Cleaning log file %LOG_FILE%...
	del %LOG_FILE%
)


REM gr: this fails on windows if $UNITY_EXE starts with a quote.
REM gr: and unity can't copy with two slashes in a path like yourdir\..//Unity
echo Making .unitypackage... using UNITY_EXE=%UNITY_EXE%
"%UNITY_EXE%" -batchmode -logFile %LOG_FILE% -projectPath %PROJECT_PATH% -exportPackage %PROJECT_PACKAGE_PATHS% %PACKAGE_FILENAME% -quit
if %ERRORLEVEL% NEQ 0	(
	REM echo Make package failed; log (%LOG_FILE%)...
	echo Make package failed
	type %LOG_FILE%
	EXIT /B 1
)

echo Make package %PACKAGE_FILENAME% succeeded
EXIT /B 0
