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

//Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwGlobalDirectory
{
public:
    cwGlobalDirectory();

    static void setupBaseDirectory();
    static QString baseDirectory();
    static QString qmlMainFilePath() { return "qml/CavewhereMainWindow.qml"; }

private:
    static QString BaseDirectory;
};

inline QString cwGlobalDirectory::baseDirectory() {
    return BaseDirectory + "/";
}

#endif // CWGLOBALDIRECTORY_H
