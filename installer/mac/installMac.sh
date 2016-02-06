#!/bin/bash

echo "Source directory" $2

echo "Cavewhere Mac Installer"

echo "Adding rpath to plotSauce"
install_name_tool -add_rpath @executable_path/../Frameworks Cavewhere.app/Contents/MacOS/plotsauce

echo "Adding rpath to cavewhere-test"
install_name_tool -add_rpath @executable_path/../Frameworks Cavewhere.app/Contents/MacOS/cavewhere-test

echo "Deleting old DMG"
rm -rv "Cavewhere $1.dmg"

echo "Creating Cavewhere DMG"
$2/mac/create-dmg/create-dmg \
    --window-size 615 379 \
    --app-drop-link 440 175 \
    --background $2/mac/dmgBackground.png \
    --icon "Cavewhere.app" 170 175 \
    "Cavewhere $1.dmg" \
    "Cavewhere.app"
