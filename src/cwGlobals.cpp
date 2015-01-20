/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwGlobals.h"

//Qt includes
#include <QFileInfo>
#include <QSettings>
#include <QDebug>

//Std includes
#include "math.h"

const double cwGlobals::PI = acos(-1.0);
const double cwGlobals::RadiansToDegrees = 180.0 / cwGlobals::PI;
const double cwGlobals::DegreesToRadians = cwGlobals::PI / 180.0;

cwGlobals::cwGlobals()
{
}

/**
  If filename doesn't have an extension, the this function will try to add the
  extensionHint to the filename.

  filename - The filename that's going to be save
  extensionHint - the extension that the file should have. Example: "txt" or "zip"

  If the file already has an extension, this return the filename
  */
QString cwGlobals::addExtension(QString filename, QString extensionHint)
{
    if(QFileInfo(filename).completeSuffix().isEmpty())  {
        return filename + "." + extensionHint;
    }

    return filename;
}

