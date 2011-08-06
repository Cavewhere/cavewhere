#ifndef CXMLPROJECTLOADTASK_H
#define CXMLPROJECTLOADTASK_H

//Our includes
#include <cwTask.h>
class cwCavingRegion;

//Qt includes
#include <QSqlDatabase>

/**
  cXMLProjectLoadTask
  */
class cwProjectIOTask : public cwTask
{
    Q_OBJECT
public:
    cwProjectIOTask(QObject* parent = NULL);

    void setDatabaseFilename(QString filename);
    QString databaseFilename() const;

protected:
    //For database access
    QString DatabasePath;
    QSqlDatabase Database; //sqlite database

    bool connectToDatabase(QString connectionName);


};

/**
  \brief Sets the filename for the task
  */
inline void cwProjectIOTask::setDatabaseFilename(QString filename) {
    DatabasePath = filename;
}

/**
  \brief Gets the database filename
  */
inline QString cwProjectIOTask::databaseFilename() const {
    return DatabasePath;
}

#endif // CXMLPROJECTLOADTASK_H
