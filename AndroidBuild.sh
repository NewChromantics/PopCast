#!/bin/sh

# external build tool uses SOURCE_ROOT instead of SRCROOT for some reason
export SRCROOT=$SOURCE_ROOT

#echo "Android env"
#env

# do makefile build
# gr: note: XCode needs to be in the Android/ dir otherwise error paths won't be relative
DEFAULT_ACTION="release"
if [ "$ACTION" == "" ]; then
	echo "Defaulting build ACTION to $DEFAULT_ACTION"
	ACTION=$DEFAULT_ACTION
fi


./build.sh $ACTION
if [ $? -ne 0 ]; then { echo "./build.sh Failed." ; exit 1; } fi

#	do nothing after clean
#	gr: maybe delete plugin?
if [ "$ACTION" == "clean" ]; then
	exit 0
fi

# move out of android dir
cd ..

# copy lib
echo "Copying .lib to plugins dir"
UNITY_PLUGINS_PATH=$SRCROOT/Unity/Assets/Plugins/
mkdir -p $UNITY_PLUGINS_PATH/Android
cp Android/libs/armeabi-v7a/* $UNITY_PLUGINS_PATH/Android
if [ $? -ne 0 ]; then { echo "copy lib Failed." ; exit 1; } fi

# copy c#
chmod +x ./CopyCSharpFiles.sh
./CopyCSharpFiles.sh
if [ $? -ne 0 ]; then { echo "./CopyCSharpFiles.sh Failed." ; exit 1; } fi

# run make package
chmod +x ./MakePackage.sh
./MakePackage.sh
if [ $? -ne 0 ]; then { echo "MakePackage.sh failed." ; exit 1; } fi


