#!/bin/sh

# external build tool uses SOURCE_ROOT instead of SRCROOT for some reason
export SRCROOT=$SOURCE_ROOT

#echo "Android env"
#env

# do makefile build
# gr: note: XCode needs to be in the Android/ dir otherwise error paths won't be relative
./build.sh release
if [ $? -ne 0 ]; then { echo "./build.sh Failed." ; exit 1; } fi

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


