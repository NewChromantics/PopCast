#!/bin/sh

export GIT_REV=`git rev-parse --verify --short HEAD`
#export PACKAGE_FILENAME="$SRCROOT/$PROJECT.$GIT_REV.unitypackage"
export PACKAGE_FILENAME="$SRCROOT/$PROJECT.unitypackage"
export PROJECT_PATH="$SRCROOT/Unity"
export UNITY_EXE="${UNITY_APP_PATH}/Unity.app/Contents/MacOS/Unity"
export LOG_FILE="./MakePackage.log"
export PROJECT_PACKAGE_PATHS="Assets/Plugins"

echo "Making .unitypackage... using Unity ${UNITY_APP_PATH}"

"$UNITY_EXE" -batchmode -logFile $LOG_FILE -projectPath $PROJECT_PATH -exportPackage $PROJECT_PACKAGE_PATHS $PACKAGE_FILENAME -quit
if [ $? -ne 0 ]; then
{
echo "Make package failed; log ($LOG_FILE)...";
cat $LOG_FILE
exit 1;
} fi

echo "Make package ${PACKAGE_FILENAME} succeeded"
