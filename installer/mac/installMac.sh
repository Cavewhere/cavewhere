#!/bin/bash

# Ensure all arguments are provided
if [[ -z "$1" || -z "$2" || -z "$3" || -z "$4" ]]; then
    echo "Error: Missing arguments."
    echo "Usage: $0 <macdeployqt_path> <email> <code_sign_id> <teamid>"
    echo "Current code sign id's:"
    security find-identity -v -p codesigning
    exit 1
fi

version=`git describe`
macdeployqt_path=$1
email=$2
codesignid=$3
team=$4

security find-identity -v -p codesigning
echo "Version:" $version
echo "macdeployqt:" $macdeployqt_path
echo "email:" $email
echo "code sign id:" $codesignid
echo "team:" $team


$macdeployqt_path CaveWhere.app -qmldir=cavewherelib -executable=CaveWhere.app/Contents/MacOS/cavewhere-test

#Code sign the bundle
find CaveWhere.app/Contents/Plugins -type f -name "*.dylib" -exec codesign --force --options runtime --deep --sign $codesignid {} \;
codesign --force --deep --options runtime --verbose --sign $codesignid CaveWhere.app
#codesign --force --deep --options runtime --verbose --sign $codesignid CaveWhere.app/Contents/MacOS/CaveWhere

#codesign --verify --deep --strict --verbose=2 CaveWhere.app

#Notrize the bundle
ditto -c -k --keepParent CaveWhere.app CaveWhere.zip
echo xcrun notarytool submit CaveWhere.zip --apple-id "$email" --team-id "$team"
xcrun notarytool submit CaveWhere.zip --apple-id "$email" --team-id "$team"

echo ""
echo "Check the submitting history with this:"
echo xcrun notarytool history --apple-id "$email" --team-id "$team" "| head"

echo ""
echo "Check the log with this:"
echo xcrun notarytool log "<id>" --apple-id "$email" --team-id "$team"

echo ""
echo "After being accepted run staple"
echo xcrun stapler staple CaveWhere.app

echo ""
echo "Verify the app"
echo spctl --assess --verbose=4 CaveWhere.app

echo ""
echo "Creating Cavewhere DMG"
echo create-dmg \
    --window-size 615 379 \
    --app-drop-link 440 175 \
    --background ../../installer/mac/dmgBackground.png \
    --icon "Cavewhere.app" 170 175 \
    --no-internet-enable \
    \""CaveWhere $version.dmg"\" \
    "CaveWhere.app"

echo ""
