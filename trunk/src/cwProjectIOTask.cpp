//Our includes
#include "cwProjectIOTask.h"

//Qt include
#include <QDebug>

cwProjectIOTask::cwProjectIOTask(QObject* parent) :
    cwTask(parent)
{
}

/**
  This connects to a mysql database

  This is a helper function to all runTask() in the subclasses
  */
bool cwProjectIOTask::connectToDatabase(QString connectionName) {
    //Connect to the database
    Database = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    Database.setDatabaseName(DatabasePath);
    bool connected = Database.open();
    if(!connected) {
        qDebug() << "Couldn't connect to database for LoadImageTask:" << DatabasePath;
        stop();
    }

    return connected;
}
