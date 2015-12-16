/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwProjectIOTask.h"
#include "cwSQLManager.h"
#include "cwDebug.h"

//Sqlite lite includes
#include <sqlite3.h>

//Qt include
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTimer>
#include <QDebug>

QAtomicInt cwProjectIOTask::DatabaseConnectionCounter;

cwProjectIOTask::cwProjectIOTask(QObject* parent) :
    cwTask(parent)
{
}

/**
 * Returns true if the io task can connect to the sqlite database with the given filename.
 * False if it can't connect to the database
 */
bool cwProjectIOTask::canConnect(QString filename)
{
    QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", "TestConnection");
    database.setDatabaseName(filename);
    bool connected = database.open();

    if(connected) {
        QSqlQuery query(database);
        connected = query.prepare("SELECT name FROM sqlite_master WHERE type = \"table\"");
    }

    database.close();
    return connected;
}

/**
 * Returns true if the io task can connect to the sqlite database with the DatabaseFilename.
 * False if it can't connect to the database
 */
bool cwProjectIOTask::canConnect() const
{
    return canConnect(databaseFilename());
}

/**
  This connects to a mysql database

  This is a helper function to all runTask() in the subclasses
  */
bool cwProjectIOTask::connectToDatabase(QString connectionName) {
    //Connect to the database
    int nextConnectonName = DatabaseConnectionCounter.fetchAndAddAcquire(1);
    Database = QSqlDatabase::addDatabase("QSQLITE", QString("%1-%2").arg(connectionName).arg(nextConnectonName));
    Database.setDatabaseName(DatabasePath);
    bool connected = Database.open();
    if(!connected) {
        qDebug() << "Couldn't connect to database for" << connectionName << DatabasePath << LOCATION;
        stop();
    }

    return connected;
}

/**
  \brief Begins a transaction for sql statements

  You should call endTransaction to end the transaction.
  */
bool cwProjectIOTask::beginTransation() {
    return cwSQLManager::instance()->beginTransaction(Database);
}

/**
  \brief Ends the transation for the sql statements

  Make sure you call beginTransaction.
  */
void cwProjectIOTask::endTransation() {
    //If the task is running
    if(isRunning()) {
        //Commit the data
        cwSQLManager::instance()->endTransaction(Database, cwSQLManager::Commit);
    } else {
        //Roll back the commited images
        cwSQLManager::instance()->endTransaction(Database, cwSQLManager::RollBack);
    }
}
