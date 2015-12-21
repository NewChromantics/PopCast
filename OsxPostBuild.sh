

# copy c#
chmod +x ./CopyCSharpFiles.sh
./CopyCSharpFiles.sh
if [ $? -ne 0 ]; then { echo "./CopyCSharpFiles.sh Failed." ; exit 1; } fi

# run make package
chmod +x ./MakePackage.sh
./MakePackage.sh
if [ $? -ne 0 ]; then { echo "MakePackage.sh failed." ; exit 1; } fi


