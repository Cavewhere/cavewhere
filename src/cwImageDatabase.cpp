//Our includes
#include "cwImageDatabase.h"
#include "cwAddImageTask.h"

//Qt includes
#include <QSqlQuery>
#include <QSqlError>
#include <QThread>
#include <QDebug>

cwImageDatabaseManager::cwImageDatabaseManager(QObject *parent) :
    QObject(parent)
{
    AddThread = new QThread(this);
    AddThread->start(QThread::LowPriority);

    AddImageTask = new cwAddImageTask();
    AddImageTask->setThread(AddThread);

    Project = NULL;

}

/**
  \brief Deconstructor
  */
cwImageDatabaseManager::~cwImageDatabaseManager() {
    AddImageTask->stop();

    //Wait until the thread stops
    QMetaObject::invokeMethod(AddThread, "quit");
    AddThread->wait();

    //Delete the addImageTask
    delete AddImageTask;
}

/**
  \brief Sets the project where this image database attached to
  */
void cwImageDatabaseManager::setProject(cwProject* project) {
    Project = project;
}

/**
  \brief This adds images to the project

  This will go through all the imagePath and load images in a non blocking way


  */
void cwImageDatabaseManager::addImages(QStringList imagePath) {
    if(Project == NULL) {
        qDebug() << "Project hasn't been set";
        return;
    }

    //AddImageTask->setDatabasePath(Project->projectPath());
    AddImageTask->setNewImagesPath(imagePath);
    AddImageTask->start();
}

///**
//  \brief This loads the image into the database


//  */
//void cwImageDatabaseManager::loadImage(QString image) {

//}


/**
  \brief This inserts the image data into the database

  The database must be open for this function to run.

  This is a low level function that loads image data into the database. Use cwLoadImageTask for
  a non blocking interface.
  */
//int cwImageDatabaseManager::addImageToDatabase(const QSqlDatabase& database, const cwImageData& imageData)  {
////    QString SQL = "INSERT INTO Images (type, shouldDelete, width, height, imageData) "
////            "VALUES (?, ?, ?, ?, ?)";

////    QSqlQuery query(database);
////    bool successful = query.prepare(SQL);

////    if(!successful) {
////        qDebug() << "Couldn't create Insert Images query: " << query.lastError();
////        return -1;
////    }

////    query.bindValue(0, imageData.format());
////    query.bindValue(1, false);
////    query.bindValue(2, imageData.size().width());
////    query.bindValue(3, imageData.size().height());
////    query.bindValue(4, imageData.data());
////    query.exec();

////    //Get the id of the last inserted id
////    return query.lastInsertId().toInt();
//}
