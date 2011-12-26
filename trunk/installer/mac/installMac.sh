#!/bin/bash

echo "Cavewhere Mac Installer"

echo "Delete rm Cavewhere"
rm -r Cavewhere.app

echo "Copying bundle"
cp -r ../../Cavewhere.app .

echo "Running macdeployqt"
macdeployqt Cavewhere.app

echo "Coping libraries"
cp /usr/lib/libGLEW.1.6.0.dylib Cavewhere.app/Contents/Frameworks/libGLEW.1.6.0.dylib
cp -r /Developer/Applications/Qt/imports/QtDesktop Cavewhere.app/Contents/MacOS

echo "Coping QML"
cp -r ../../qml Cavewhere.app/Contents/MacOS

echo "Coping Shaders"
cp -r ../../shaders Cavewhere.app/Contents/MacOS


echo "Installing name tool on glew"
install_name_tool -change /usr/lib/libGLEW.1.6.0.dylib @executable_path/../Frameworks/libGLEW.1.6.0.dylib Cavewhere.app/Contents/MacOS/Cavewhere

echo "Installing name tool on plotSauce"
install_name_tool -change QtXml.framework/Versions/4/QtXml @executable_path/../Frameworks/QtXml.framework/Versions/4/QtXml Cavewhere.app/Contents/MacOS/plotsauce
install_name_tool -change QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore Cavewhere.app/Contents/MacOS/plotsauce
install_name_tool -change QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui Cavewhere.app/Contents/MacOS/plotsauce

echo "Installing name tool on qt desktop"
install_name_tool -change QtDeclarative.framework/Versions/4/QtDeclarative @executable_path/../Frameworks/QtDeclarative.framework/Versions/4/QtDeclarative Cavewhere.app/Contents/MacOS/QtDesktop/plugin/libstyleplugin.dylib
install_name_tool -change QtScript.framework/Versions/4/QtScript @executable_path/../Frameworks/QtScript.framework/Versions/4/QtScript Cavewhere.app/Contents/MacOS/QtDesktop/plugin/libstyleplugin.dylib
install_name_tool -change QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore Cavewhere.app/Contents/MacOS/QtDesktop/plugin/libstyleplugin.dylib
install_name_tool -change QtSvg.framework/Versions/4/QtSvg @executable_path/../Frameworks/QtSvg.framework/Versions/4/QtSvg Cavewhere.app/Contents/MacOS/QtDesktop/plugin/libstyleplugin.dylib
install_name_tool -change QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui Cavewhere.app/Contents/MacOS/QtDesktop/plugin/libstyleplugin.dylib
install_name_tool -change QtSql.framework/Versions/4/QtSql @executable_path/../Frameworks/QtSql.framework/Versions/4/QtSql Cavewhere.app/Contents/MacOS/QtDesktop/plugin/libstyleplugin.dylib
install_name_tool -change QtXmlPatterns.framework/Versions/4/QtXmlPatterns @executable_path/../Frameworks/QtXmlPatterns.framework/Versions/4/QtXmlPatterns Cavewhere.app/Contents/MacOS/QtDesktop/plugin/libstyleplugin.dylib
install_name_tool -change QtNetwork.framework/Versions/4/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork Cavewhere.app/Contents/MacOS/QtDesktop/plugin/libstyleplugin.dylib
