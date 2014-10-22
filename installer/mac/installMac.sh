#!/bin/bash

qtDir=$2

echo "Cavewhere Mac Installer"

echo "Installing name tool on plotSauce"
install_name_tool -change $qtDir/QtXml.framework/Versions/5/QtXml @executable_path/../Frameworks/QtXml.framework/Versions/5/QtXml Cavewhere.app/Contents/MacOS/plotsauce
install_name_tool -change $qtDir/QtCore.framework/Versions/5/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore Cavewhere.app/Contents/MacOS/plotsauce
install_name_tool -change $qtDir/QtGui.framework/Versions/5/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui Cavewhere.app/Contents/MacOS/plotsauce

pwd
echo "Zipping Cavewhere"
zip -r "Cavewhere $1.zip" "Cavewhere.app"
