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
#include <QApplication>
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

/**
 * @brief cwGlobals::convertFromURL
 * @param fileUrl - The url that will be convert
 * @return Returns the converted url
 *
 * For example if fileUrl = file://SOME/LOCAL/FILENAME with will convert it to //SOME/LOCAL/FILENAME
 *
 * If the filenameUrl isn't a url, this just returns filenameUrl
 */
QString cwGlobals::convertFromURL(QString filenameUrl)
{
    QUrl fileUrl(filenameUrl);
    if(fileUrl.isValid() && fileUrl.isLocalFile()) {
        return fileUrl.toLocalFile();
    }
    return filenameUrl;
}

/**
 * @brief cwGlobals::findExecutable
 * @param executables - A list of executables, [cavern.exe, cavern]
 * @return Returns the first exectuable that exists base on the QApplication::applicationPath()
 */
QString cwGlobals::findExecutable(QStringList executables)
{
    QString execPath;

    foreach(QString plotSauceAppName, executables) {
        QDir plotSauceDir(QApplication::applicationDirPath());
        QString currentPlotSaucePath = plotSauceDir.absoluteFilePath(plotSauceAppName);

        QFileInfo plotSauceFileInfo(currentPlotSaucePath);
        if(plotSauceFileInfo.exists() && plotSauceFileInfo.isExecutable()) {
            execPath = currentPlotSaucePath;
            break;
        }
    }

    return execPath;
}

