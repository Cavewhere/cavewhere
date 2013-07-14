/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwProjectIOTask.h"
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

  restartSlot is called when the database is busy with another transaction. By default this is
  null. If it's null no slot is called by a time, the function simply returns with an error message, and
  returns false.  If the database is busy, this also returns false.  If this function returns false, this
  means that other SQL functions should run.

  You should call endTransaction to end the transaction.
  */
bool cwProjectIOTask::beginTransation(const char* restartSlot) {
    //SQLITE begin transation
    QString beginTransationQuery = "BEGIN IMMEDIATE TRANSACTION";
    QSqlQuery query = Database.exec(beginTransationQuery);
    QSqlError error = query.lastError();

    //Check if there's error
    if(error.isValid()) {
        if(error.number() == SQLITE_BUSY) {
            if(restartSlot != NULL) {
                //The database is busy, try to get a lock in 500ms
                QTimer::singleShot(500, this, restartSlot);
            } else {
                qDebug() << "Can't begin transaction because the database is busy" << LOCATION;
            }
            return false;
        } else {
            qDebug() << "Database error whene trying to begin transaction: " << error << LOCATION;
            return false;
        }
    }

    return true;
}

/**
  \brief Ends the transation for the sql statements

  Make sure you call beginTransaction.
  */
void cwProjectIOTask::endTransation() {
    //If the task is running
    if(isRunning()) {
        //Commit the data
        QString commitTransationQuery = "COMMIT";
        QSqlQuery query = Database.exec(commitTransationQuery);
        if(query.lastError().isValid()) {
            qDebug() << "Couldn't commit transaction:" << query.lastError() << LOCATION;
        }
    } else {
        //Roll back the commited images
        QString rollbackTransationQuery = "ROLLBACK";
        QSqlQuery query = Database.exec(rollbackTransationQuery);
        if(query.lastError().isValid()) {
            qDebug() << "Couldn't rollback transaction:" << query.lastError() << LOCATION;
        }
    }
}
