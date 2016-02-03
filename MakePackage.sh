#!/bin/sh

export GIT_REV=`git rev-parse --verify --short HEAD`
#export PACKAGE_FILENAME="$SRCROOT/$PROJECT.$GIT_REV.unitypackage"
export PACKAGE_FILENAME="$SRCROOT/$PROJECT.unitypackage"
export PROJECT_PATH="$SRCROOT/Unity"

# windows build already exports UNITY_EXE specifically
if [ -z "$UNITY_EXE" ]; then
{
	export UNITY_EXE="${UNITY_APP_PATH}/Unity.app/Contents/MacOS/Unity"
} fi

export LOG_FILE="./MakePackage.log"
export PROJECT_PACKAGE_PATHS="Assets/Plugins"

echo "Making .unitypackage... using UNITY_EXE=${UNITY_EXE}"
echo "PROJECT_PATH=${PROJECT_PATH}"
echo "LOG_FILE=${LOG_FILE}"

# clean log file first
if [ -f $LOG_FILE ]; then
{
	echo "Cleaning log file $LOG_FILE..."
	rm $LOG_FILE
} fi

# gr: this fails on windows if $UNITY_EXE starts with a quote.
# gr: and unity can't copy with two slashes in a path like yourdir\..//Unity
"$UNITY_EXE" -batchmode -logFile $LOG_FILE -projectPath $PROJECT_PATH -exportPackage $PROJECT_PACKAGE_PATHS $PACKAGE_FILENAME -quit
if [ $? -ne 0 ]; then
{
	echo "Make package failed; log ($LOG_FILE)...";
	cat $LOG_FILE
	exit 1;
} fi

echo "Make package ${PACKAGE_FILENAME} succeeded"
