#ifndef CWIMAGEDATABASE_H
#define CWIMAGEDATABASE_H

//Qt includes
#include <QObject>

//Our includes
#include "cwImageData.h"
#include "cwProject.h"
#include "cwAddImageTask.h"

class cwImageDatabaseManager : public QObject
{
    Q_OBJECT
public:
    explicit cwImageDatabaseManager(QObject *parent = 0);
    ~cwImageDatabaseManager();

    //Where the image data will be stored
    void setProject(cwProject* project);

    //Adds the images to the database
    void addImages(QStringList imagePath);

    //Adds a single image to the database
//    static int addImageToDatabase(const QSqlDatabase& database, const cwImageData& data);

signals:

public slots:

private:
    cwProject* Project; //Where the images are stored

    //For add image data to database
    QThread* AddThread;
    cwAddImageTask* AddImageTask;

    QList<QImage> loadImage(QString image);
};

#endif // CWIMAGEDATABASE_H
