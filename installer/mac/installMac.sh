#!/bin/bash

set -e

echo "Source directory" $2

echo "Code Sign" $3

echo "Cavewhere Mac Installer"

echo "Adding rpath to plotSauce"
install_name_tool -add_rpath @executable_path/../Frameworks Cavewhere.app/Contents/MacOS/CaveWhere

echo "Adding rpath to plotSauce"
install_name_tool -add_rpath @executable_path/../Frameworks Cavewhere.app/Contents/MacOS/plotsauce

>&2 echo "Adding rpath to cavewhere-test"
>&2 echo "$4/lib/cavewhere-lib.framework/Versions/A/cavewhere-lib"
install_name_tool -add_rpath @executable_path/../Frameworks Cavewhere.app/Contents/MacOS/cavewhere-test
install_name_tool -change $4/lib/cavewhere-lib.framework/Versions/A/cavewhere-lib @executable_path/../Frameworks/cavewhere-lib.framework/Versions/A/cavewhere-lib Cavewhere.app/Contents/MacOS/cavewhere-test
install_name_tool -change $4/lib/s3tc-dxt-decompression.framework/Versions/A/s3tc-dxt-decompression @executable_path/../Frameworks/s3tc-dxt-decompression.framework/Versions/A/s3tc-dxt-decompression Cavewhere.app/Contents/MacOS/cavewhere-test
install_name_tool -change $4/lib/dewalls.framework/Versions/A/dewalls @executable_path/../Frameworks/dewalls.framework/Versions/A/dewalls Cavewhere.app/Contents/MacOS/cavewhere-test
install_name_tool -change $4/lib/QMath3d.framework/Versions/A/QMath3d @executable_path/../Frameworks/QMath3d.framework/Versions/A/QMath3d Cavewhere.app/Contents/MacOS/cavewhere-test

echo "Deleting old DMG"
rm -rvf "CaveWhere $1.dmg"

echo "Code Signing"
codesign --deep --force --verify --verbose --sign "$3" --options runtime CaveWhere.app

echo "Verifying Signing"
codesign --verify --verbose=4 CaveWhere.app

echo "Creating Cavewhere DMG"
$2/mac/create-dmg/create-dmg \
    --window-size 615 379 \
    --app-drop-link 440 175 \
    --background $2/mac/dmgBackground.png \
    --icon "Cavewhere.app" 170 175 \
    "CaveWhere $1.dmg" \
    "CaveWhere.app"
