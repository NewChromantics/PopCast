
# Xcode seems to run this script BEFORE copying output files to /Unity... whch means the package is done without the latest binary... do it here (like IOS)
echo "Copying $CODESIGNING_FOLDER_PATH to Unity dir"
TARGET_PATH="$SRCROOT/Unity/Assets/Plugins/Osx"
mkdir -p $TARGET_PATH
cp -a "$CODESIGNING_FOLDER_PATH" $TARGET_PATH
if [ $? -ne 0 ]; then { echo "Copy binary failed." ; exit 1; } fi

# copy c#
chmod +x ./CopyCSharpFiles.sh
./CopyCSharpFiles.sh
if [ $? -ne 0 ]; then { echo "./CopyCSharpFiles.sh Failed." ; exit 1; } fi

# run make package
chmod +x ./MakePackage.sh
./MakePackage.sh
if [ $? -ne 0 ]; then { echo "MakePackage.sh failed." ; exit 1; } fi


