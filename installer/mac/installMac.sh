#!/bin/bash

# Ensure all arguments are provided
if [[ -z "$1" || -z "$2" || -z "$3" || -z "$4" || -z "$5" ]]; then
    echo "Error: Missing arguments."
    echo "Usage: $0 <version> <macdeployqt_path> <email> <code_sign_id> <teamid>"
    echo "Current code sign id's:"
    security find-identity -v -p codesigning
    exit 1
fi

security find-identity -v -p codesigning
echo "Version:" $1
echo "macdeployqt:" $2
echo "email:" $3
echo "code sign id:" $4
echo "team:" $5


$2 CaveWhere.app -qmldir=cavewherelib #-sign-for-notarization=$3

#Code sign the bundle
find CaveWhere.app/Contents/Plugins -type f -name "*.dylib" -exec codesign --force --options runtime --deep --sign $4 {} \;
codesign --force --deep --options runtime --verbose --sign $4 CaveWhere.app
#codesign --force --deep --options runtime --verbose --sign $4 CaveWhere.app/Contents/MacOS/CaveWhere

#codesign --verify --deep --strict --verbose=2 CaveWhere.app

#Notrize the bundle
ditto -c -k --keepParent CaveWhere.app CaveWhere.zip
echo xcrun notarytool submit CaveWhere.zip --apple-id "$3" --team-id "$5"
xcrun notarytool submit CaveWhere.zip --apple-id "$3" --team-id "$5"

echo ""
echo "Check the submitting history with this:"
echo xcrun notarytool history --apple-id "$3" --team-id "$5"

echo ""
echo "Check the log with this:"
echo xcrun notarytool log "<id>" --apple-id "$3" --team-id "$5"

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
    '"CaveWhere $1.dmg"' \
    "CaveWhere.app"

echo ""
