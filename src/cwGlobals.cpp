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

/**
 * @brief cwGlobals::openFile
 * @param caption
 * @param qSettingsKey
 * @param filter
 * @return
 */
QString cwGlobals::openFile(QString caption, QString filter, QString qSettingsKey, QFileDialog::Options options)
{
    QString lastDirectory;
    QSettings settings;

    if(!qSettingsKey.isEmpty()) {
        lastDirectory = settings.value(qSettingsKey).toString();
        qDebug() << "Getting lastDirectory:" << lastDirectory;
    }

    QString openedFile = QFileDialog::getOpenFileName(NULL, caption, lastDirectory, filter, 0, options);

    QFileInfo fileInfo(openedFile);
    if(!qSettingsKey.isEmpty() && fileInfo.exists())
    {
        qDebug() << "Setting value:" << qSettingsKey << fileInfo.absolutePath();
        settings.setValue(qSettingsKey, fileInfo.absolutePath());
        settings.sync();
    }

    return openedFile;
}

/**
 * @brief cwGlobals::openFiles
 * @param caption
 * @param filter
 * @param qSettingsKey
 * @return
 */
QStringList cwGlobals::openFiles(QString caption, QString filter, QString qSettingsKey, QFileDialog::Options options)
{
    QString lastDirectory;
    QSettings settings;

    if(!qSettingsKey.isEmpty()) {
        lastDirectory = settings.value(qSettingsKey).toString();
    }

    QStringList openedFiles = QFileDialog::getOpenFileNames(NULL, caption, lastDirectory, filter, 0, options);


    if(!openedFiles.isEmpty() && !qSettingsKey.isEmpty())
    {
        QFileInfo fileInfo(openedFiles.first());
        if(fileInfo.exists()) {
            settings.setValue(qSettingsKey, fileInfo.absolutePath());
            settings.sync();
        }
    }

    return openedFiles;
}

