//Our includes
#include "cwGlobalDirectory.h"

//Qt includes
#include <QFileInfo>
#include <QDir>
#include <QApplication>
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
    QString findFile = "/qml/CavewhereMainWindow.qml";

    //  QApplication::

    QString currentDirectory = QDir::currentPath();
    QString execDirectory = QApplication::applicationDirPath();

    bool currentExists = QFileInfo(currentDirectory + findFile).exists();
    bool execExists = QFileInfo(execDirectory + findFile).exists();

    if(currentExists) {
        BaseDirectory = currentDirectory;
    } else if(execExists) {
        BaseDirectory = execDirectory;
    } else {
        qDebug() << "CurrentDirectory:" << (currentDirectory + findFile) << "exists:" << currentExists;
        qDebug() << "ExecDirectory:" << (execDirectory + findFile) << "exists:" << execExists;
        qFatal("Couldn't find qml/CavewhereMainWindow.qml, installed wrong!?");
    }
}
