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
#include "cwProject.h"

//Qt include
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTimer>
#include <QDebug>

//Std includes

QAtomicInt cwProjectIOTask::DatabaseConnectionCounter;

cwProjectIOTask::cwProjectIOTask(QObject* parent) :
    cwTask(parent)
{
}

QList<cwError> cwProjectIOTask::errors() const
{
    return Errors;
}

QSqlDatabase cwProjectIOTask::createDatabase(const QString &connectionName, const QString& databasePath)
{
    return cwProject::createDatabaseConnection(connectionName, databasePath);
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
        addError(cwError(QString("Couldn't connect to database for %1 %2 %3").arg(connectionName).arg(DatabasePath), cwError::Fatal));
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

void cwProjectIOTask::addError(const cwError &error)
{
    Errors.append(error);
}

void cwProjectIOTask::clearErrors()
{
    Errors.clear();
}
