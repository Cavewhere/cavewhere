/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Qt includes
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDatabase>
#include <QThread>

//Our includse
#include "cwSQLManager.h"
#include "cwDebug.h"

//This hold the singleton of the cwSQLManager
cwSQLManager* cwSQLManager::Instance = new cwSQLManager();

cwSQLManager::cwSQLManager(QObject *parent) :
    QObject(parent)
{
}

/**
 * @brief cwSQLManager::instance
 * @return The signalton instance of this class
 */
cwSQLManager *cwSQLManager::instance()
{
    return Instance;
}


/**
 * @brief cwSQLManager::beginTransaction
 *
 * This begins an exclusive connection to the database. This will prevent SQL_BUSY errors.
 * Although the SQLITE database is thread safe, it simply return SQL_BUSY. beginTransaction() and
 * endTransaction() insure that database is ready for reading and writing, we'll never get
 * at SQL_BUSY error.
 *
 * Make sure to call endTransaction() after preforming sql queries.
 *
 * Warning Calling the function multiple times in the same thread, with out calling endTransaction()
 * will cause deadlock!
 *
 * Returns false if the was an SQL error and prints out that error
 *
 * @param database - The database that this will start a connection with
 * @param type - ReadOnly should only be used when using select queries. It is safe to use WriteRead
 * for all queries, including SELECT statements. WriteRead will block other calls with ReadOnly and
 * WriteRead and make the function synconous. If ReadOnly is specified, other ReadOnly transactions
 * can occur in different threads. This property control a QReadWriteLock.
 *
 */
bool cwSQLManager::beginTransaction(const QSqlDatabase& database, QueryType type)
{
    QReadWriteLock* lock = fetchDatabaseMutex(database.databaseName());
    switch(type) {
    case ReadOnly:
        lock->lockForRead();
        break;
    case WriteRead:
        lock->lockForWrite();
        break;
    }

    const int SQLITE_BUSY = 5; //From the sqlite header, this prevents us from including sqlite

    //SQLITE begin transation
    QString beginTransationQuery = "BEGIN TRANSACTION";
    QSqlQuery query(database);
    query.exec(beginTransationQuery);
    QSqlError error = query.lastError();
    while (error.isValid() && error.nativeErrorCode().toInt() == SQLITE_BUSY) {
        //Try to wait until the database becomes less busy
        QThread::currentThread()->sleep(250);
        query.exec(beginTransationQuery);
        error = query.lastError();
    }

    if(error.isValid()) {
        if(error.nativeErrorCode().toInt() != SQLITE_BUSY) {
            //Some other error
            qDebug() << "Database error when trying to begin transaction:" << error << error.text() << LOCATION;
            return false;
        }
    }
    return true;
}

/**
 * @brief cwSQLManager::endTransaction
 * @param database
 * @param type - Commit will commit the transaction, and RollBack will cancel the transaction.
 *
 * This ends the transaction for thread. This will unlock the mutex that protects the database.
 */
void cwSQLManager::endTransaction(const QSqlDatabase &database, cwSQLManager::EndType type)
{
    QString commitTransationQuery;
    switch(type) {
    case Commit:
        commitTransationQuery = "COMMIT";
        break;
    case RollBack:
        commitTransationQuery = "ROLLBACK";
        break;
    default:
        Q_ASSERT(false);
    }

    QSqlQuery query(database);
    query.exec(commitTransationQuery);
    if(query.lastError().isValid()) {
        qDebug() << "Couldn't" << commitTransationQuery << "transaction:" << query.lastError() << LOCATION;
    }

    QReadWriteLock* lock = fetchDatabaseMutex(database.databaseName());
    lock->unlock();
}

/**
 * @brief cwSQLManager::fetchDatabaseMutex
 * @param databaseName - The name of the database
 * @return Returns a mutex for a databaseFile.
 */
QReadWriteLock* cwSQLManager::fetchDatabaseMutex(QString databaseName)
{
   QMutexLocker locker(&DatabaseHashMutex);
   if(!DatabaseNameToMutexIndex.contains(databaseName)) {
       DatabaseNameToMutexIndex.insert(databaseName, new QReadWriteLock());
   }

   return DatabaseNameToMutexIndex.value(databaseName, nullptr);
}

/**
 * @brief cwSQLManager::Transaction::Transaction
 *
 * This is similar to QMutexLocker. When the Transaction object is created, it begin's a
 * transaction on the database.
 *
 */
cwSQLManager::Transaction::Transaction(const QSqlDatabase& database, QueryType type) :
    Database(database),
    RolledBack(false)
{
    cwSQLManager::instance()->beginTransaction(Database, type);
}

/**
 * @brief cwSQLManager::Transaction::~Transaction
 *
 * When the transaction is deleted, this will end the transaction, with commit. If the
 * transaction was rolledBack, this will do nothing.
 */
cwSQLManager::Transaction::~Transaction()
{
    if(!RolledBack) {
        cwSQLManager::instance()->endTransaction(Database, cwSQLManager::Commit);
    }
}

/**
 * @brief cwSQLManager::Transaction::rollBack
 *
 * This will end the transcation with a RollBack ie. cancell the transaction
 */
void cwSQLManager::Transaction::rollBack()
{
    cwSQLManager::instance()->endTransaction(Database, cwSQLManager::Commit);
    RolledBack = true;
}

