#ifndef CWLOADIMAGETASK_H
#define CWLOADIMAGETASK_H

//Our includes
#include "cwTask.h"
#include "cwImage.h"

//Qt includes
#include <QStringList>
#include <QString>
#include <QImage>
#include <QSqlDatabase>

class cwAddImageTask : public cwTask
{
    Q_OBJECT

public:
    cwAddImageTask(QObject* parent = NULL);

    void setDatabasePath(const QString& databasePath);
    void setImagesPath(QStringList imagePaths);

    QList<cwImage> images();

    //static int addImageToDatabase(QSqlDatabase& database, const cwImageData& data);

protected:
    virtual void runTask();

private:
    QStringList ImagePaths;
    QString DatabasePath;

    QList<cwImage> Images;

    QSqlDatabase CurrentDatabase;


    /**
      This will scale an image to size

      This class is used with Qt::Concurrent to scale a single image multiple times
      */
    class Scaler {
    public:
        Scaler(const QImage& image) { OriginalImage = image; }

        QImage operator()(int maxSideInPixels);

    private:
        QImage OriginalImage;
    };

private slots:
    void tryAddingmagesToDatabase();


};

/**
  \brief Sets the databasePath

  This should be set before calling start!  And shouldn't be changed until the task
  has finished
  */
inline void cwAddImageTask::setDatabasePath(const QString& databasePath) {
    DatabasePath = databasePath;
}

/**
  \brief Sets the databasePath

  This should be set before calling start!  And shouldn't be changed until the task
  has finished
  */
inline void cwAddImageTask::setImagesPath(QStringList imagePaths) {
    ImagePaths = imagePaths;
}

/**
  Get's all the images that have been put into the database

  \brief This should only be called when the task is not running
  */
inline QList<cwImage> cwAddImageTask::images() {
    return Images;
}

#endif // CWLOADIMAGETASK_H
