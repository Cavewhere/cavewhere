/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWSQLMANAGER_H
#define CWSQLMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QHash>
#include <QMap>
#include <QMutex>
#include <QReadWriteLock>

/**
 * @brief The cwSQLManager class
 *
 * This is as singleton class. To use the class use cwSQLManager::instance()
 *
 * This class uses a mutex to make all sql transactions synchronous. Because of this, it may cause
 * the calling thread to block. The mutrex is locked when calling beginTransaction() and unlocked
 * when calling endTransaction(). If another application is using the database, and SQL_BUSY error
 * happends, this class will try wait 250ms and try to begin a transaction. Using QSqlQuery outside
 * of this class can cause SQL_BUSY errors.
 *
 * This class allows for multiple database (databases in different files) to be handled correctly.
 * This class will not block access to multiple databases and can be read and written to asynchronously.
 */
class cwSQLManager : public QObject
{
    Q_OBJECT
public:    
    enum QueryType {
        ReadOnly,
        WriteRead
    };

    enum EndType {
        Commit,
        RollBack
    };

    class Transaction {
    public:
        Transaction(const QSqlDatabase *database, QueryType type = WriteRead);
        ~Transaction();

        void rollBack();

    private:
        const QSqlDatabase* Database;
        bool RolledBack;

        //Disabled from copying
        Transaction(const Transaction &t) { Q_UNUSED(t); Q_ASSERT(false); }
        Transaction& operator=(const Transaction& t) { Q_UNUSED(t); Q_ASSERT(false); return *this; }
    };

    static cwSQLManager* instance();

    bool beginTransaction(const QSqlDatabase& database, QueryType type = WriteRead);
    void endTransaction(const QSqlDatabase& database, EndType type = Commit);

signals:

public slots:

private:
    explicit cwSQLManager(QObject *parent = 0);

    static cwSQLManager* Instance;

    QReadWriteLock *fetchDatabaseMutex(QString databaseFile);

    //This protects the two structures below
    QMutex DatabaseHashMutex;

    //Converts a DatabaseName into a Mutex to protect the database
    QHash<QString, QReadWriteLock*> DatabaseNameToMutexIndex;

};

#endif // CWSQLMANAGER_H
