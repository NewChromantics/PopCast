

echo "Copying c# to Unity dir"
UNITY_PLUGINS_PATH="$SRCROOT/Unity/Assets/$PROJECT"
mkdir -p $UNITY_PLUGINS_PATH

# gr: using quotes with *.cs fails - file not found...
cp $SRCROOT/src/*.cs $UNITY_PLUGINS_PATH
if [ $? -ne 0 ]; then { echo "copy c# Failed." ; exit 1; } fi
cp -r "$SRCROOT/src/Editor" $UNITY_PLUGINS_PATH
if [ $? -ne 0 ]; then { echo "copy c# Failed." ; exit 1; } fi

# replace GIT_REVISION(the string) with the git revision in the copied file(GIT_REV, the variable)
# can't seem to catch when this fails (ie, cant find git.exe)
export GIT_REV=`git rev-parse --verify --short HEAD`
if [ -z "${GIT_REV+1}"  ]; then
{
	export GIT_REV=GIT_NOT_FOUND
} fi

# this is the string we're going to match
export GIT_REV_MATCH=GIT_REVISION


if [ -n "${TARGET_WINDOWS+1}"  ]; then
{
	echo "Skipping (windows has no sed) - Replacing $GIT_REV_MATCH in C# with $GIT_REV..."
}
else
{
	echo "Replacing $GIT_REV_MATCH in C# with $GIT_REV..."
	# streamedit -inplace <backupextension> -e command
	# gr: using quotes with *.cs fails - file not found here too...
	sed -i '' -e "s/$GIT_REV_MATCH/$GIT_REV/g" $UNITY_PLUGINS_PATH/*.cs
	# gr: on windows, sed not known... error code still 0 :/
	if [ $? -ne 0 ]; then
	{
		echo "Warning, updating $GIT_REV_MATCH Failed." ;
	} fi
}
fi

