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

    auto path = qgetenv("PATH");
    auto paths = QString::fromLocal8Bit(path).split(';');
    paths.prepend(QApplication::applicationDirPath());

    foreach(QString appName, executables) {
        for(const auto& directory : paths) {
            QDir appDir(directory);
            QString currentPath = appDir.absoluteFilePath(appName);

            QFileInfo fileInfo(currentPath);
            if(fileInfo.exists() && fileInfo.isExecutable()) {
                execPath = currentPath;
                break;
            }
        }
    }

    return execPath;
}


double cwGlobals::pi()
{
    static const double pi = acos(-1.0);
    return pi;
}
