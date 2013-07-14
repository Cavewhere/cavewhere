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

QString cwGlobalDirectory::BaseDirectory;

cwGlobalDirectory::cwGlobalDirectory()
{
}


/**
  Sets up the base directory

  This tries to find files that should exist in the base directory.

  It will first check the working directory, and the check the executable path.
  */
void cwGlobalDirectory::setupBaseDirectory() {
    QString findFile = "qml/CavewhereMainWindow.qml";

    //  QApplication::

    QString currentDirectory = QDir::currentPath();
    QString execDirectory = QApplication::applicationDirPath();

    bool currentExists = QFileInfo(currentDirectory + "/" + findFile).exists();
    bool execExists = QFileInfo(execDirectory + "/" + findFile).exists();

    if(currentExists) {
        BaseDirectory = currentDirectory;
    } else if(execExists) {
        BaseDirectory = execDirectory;
    } else {
        qDebug() << "CurrentDirectory:" << (currentDirectory + "/" + findFile) << "exists:" << currentExists;
        qDebug() << "ExecDirectory:" << (execDirectory + "/" + findFile) << "exists:" << execExists;
        qDebug() << "Couldn't find qml/CavewhereMainWindow.qml, installed wrong!?";

        QMessageBox::critical(NULL,
                              "Installation is Broke Sauce",
                              QString("<b>Installation is broken!!!</b><br><br>Cavewhere couldn't find <i>%1</i>").arg(findFile),
                              QMessageBox::Close
                              );

        exit(1);
    }
}
