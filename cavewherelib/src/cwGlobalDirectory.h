/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGLOBALDIRECTORY_H
#define CWGLOBALDIRECTORY_H

//Qt includes
#include <QString>
#include <QUrl>

//Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwGlobalDirectory
{
public:
    cwGlobalDirectory();

    static void setupBaseDirectory();
    static QString resourceDirectory();
    static QString qmlMainFilePath() { return QStringLiteral("qrc:/qt/qml/CavewhereMainWindow.qml"); }
    static QUrl mainWindowSourcePath();

private:
    static QString ResourceDirectory;
};

inline QString cwGlobalDirectory::resourceDirectory() {
    return ResourceDirectory + "/";
}

#endif // CWGLOBALDIRECTORY_H
