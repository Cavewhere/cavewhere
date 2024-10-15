/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwGlobalDirectory.h"

//Qt includes
#include <QFileInfo>
#include <QDir>
#include <QApplication>
#include <QMessageBox>
#include <QDebug>

QString cwGlobalDirectory::ResourceDirectory;

cwGlobalDirectory::cwGlobalDirectory()
{
}


/**
  Sets up the base directory

  This tries to find files that should exist in the base directory.

  It will first check the working directory, and the check the executable path.
  */
void cwGlobalDirectory::setupBaseDirectory() {
    QString findFile = qmlMainFilePath();

    auto resourceDirectory = []() {
#if defined(Q_OS_WIN)
    return QApplication::applicationDirPath() + "/";
#elif defined(Q_OS_OSX)
    return QApplication::applicationDirPath() + "/../Resources/";
#elif defined(Q_OS_LINUX)
    return QApplication::applicationDirPath() + "/";
#else
    return QApplication::applicationDirPath() + "/";
#endif
    };

    QString currentDirectory = QString(CAVEWHERE_SOURCE_DIR);
    QString resourcesDir = resourceDirectory();

    bool currentExists = QFileInfo(currentDirectory + "/" + findFile).exists();
    bool resourcesExists = QFileInfo(resourcesDir + "/" + findFile).exists();

    if(currentExists) {
        ResourceDirectory = currentDirectory;
    } else if(resourcesExists) {
        ResourceDirectory = resourcesDir;
    } else {
        qDebug() << "CurrentDirectory:" << (currentDirectory + "/" + findFile) << "exists:" << currentExists;
        qDebug() << "ExecDirectory:" << (resourcesDir + "/" + findFile) << "exists:" << resourcesExists;
        qDebug() << "Couldn't find qml/CavewhereMainWindow.qml, installed wrong!?";

        QMessageBox::critical(nullptr,
                              "Installation is Broke Sauce",
                              QString("<b>Installation is broken!!!</b><br><br>CaveWhere couldn't find <i>%1</i>").arg(findFile),
                              QMessageBox::Close
                              );

    }
}

QUrl cwGlobalDirectory::mainWindowSourcePath() {
    // return QUrl::fromLocalFile(cwGlobalDirectory::resourceDirectory() + cwGlobalDirectory::qmlMainFilePath());
    return cwGlobalDirectory::qmlMainFilePath();
}
