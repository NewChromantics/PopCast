

echo "Copying c# to Unity dir"
UNITY_PLUGINS_PATH="$SRCROOT/Unity/Assets/Plugins"
mkdir -p $UNITY_PLUGINS_PATH
cp "$SRCROOT/src/$PROJECT.cs" $UNITY_PLUGINS_PATH
if [ $? -ne 0 ]; then { echo "copy c# Failed." ; exit 1; } fi

# replace GIT_REVISION with the git revision in the copied file

export GIT_REV=`git rev-parse --verify --short HEAD`
export GIT_REV_MATCH=GIT_REVISION


if [ -n "${TARGET_WINDOWS+1}"  ]; then
{
	echo "Skipping (windows has no sed) - Replacing $GIT_REV_MATCH in C# with $GIT_REV..."
}
else
{
	echo "Replacing $GIT_REV_MATCH in C# with $GIT_REV..."
	# streamedit -inplace <backupextension> -e command
	sed -i '' -e "s/$GIT_REV_MATCH/$GIT_REV/g" "$UNITY_PLUGINS_PATH/$PROJECT.cs"
	# gr: on windows, sed not known... error code still 0 :/
	if [ $? -ne 0 ]; then
	{
		echo "Warning, updating $GIT_REV_MATCH Failed." ;
	} fi
}
fi

