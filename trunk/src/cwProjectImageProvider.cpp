//Our includes
#include "cwProjectImageProvider.h"

//Qt includes
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlField>
#include <QSqlRecord>
#include <QFileInfo>
#include <QVariant>
#include <QMutexLocker>
#include <QDebug>

const QString cwProjectImageProvider::Name = "sqlimagequery";
const QString cwProjectImageProvider::RequestImageSQL = "SELECT type,imageData from Images where id=?";

cwProjectImageProvider::cwProjectImageProvider() :
    QDeclarativeImageProvider(QDeclarativeImageProvider::Image)
{



}

/**
  \brief This extracts a image from the database

  See Qt docs for details
  */
QImage cwProjectImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
    //Set the default size
    *size = QSize(0, 0);

    bool okay;
    int sqlId = id.toInt(&okay);

    if(!okay) {
        qDebug() << "cwProjectImageProvider:: Couldn't convert id to a number where id=" << id;
        return QImage();
    }

    if(!QFileInfo(ProjectPath).isFile()) {
        qDebug() << "cwProjectImageProvider:: ProjectPath isn't set or isn't a file:" << ProjectPath;
        return QImage();
    }

    //Define the database
    QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", QString("resquestImage/%1").arg(id));
    database.setDatabaseName(projectPath());

    //Create an sql connection
    bool connected = database.open();
    if(!connected) {
        qDebug() << "cwProjectImageProvider:: Couldn't connect to database:" << ProjectPath;
        return QImage();
    }

    //Setup the query
    QSqlQuery query(database);
    bool successful = query.prepare(RequestImageSQL);

    if(!successful) {
        qDebug() << "cwProjectImageProvider:: Couldn't prepare query " << RequestImageSQL;
        return QImage();
    }

    //Set the id that we're searching for
    query.bindValue(0, sqlId);
    query.exec();

    //Extract the data
    QSqlRecord record = query.record();
    if(record.isEmpty()) {
        qDebug() << "cwProjectImageProvider:: No data extracted for " << id;
        return QImage();
    }

    //Get the first row
    query.next();

    QByteArray type = query.value(0).toByteArray();
    QByteArray imageData = query.value(1).toByteArray();

    qDebug() << "Number of fields:" << record.count() << record.field(0) << record.field(1);

    //Read the image in
    QImage image = QImage::fromData(imageData, type);

    //Make sure the image is good
    if(image.isNull()) {
        qDebug() << "cwProjectImageProvider:: Image isn't of format " << type;
        return QImage();
    }

    int maxSize = qMax(requestedSize.width(), requestedSize.height());

    //Get the size of the image, scale it and return it.
    QImage scaledImage = image.scaled(QSize(maxSize, maxSize), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    *size = scaledImage.size();
    qDebug() << "size of scaleImage:" << scaledImage.size();

    return scaledImage;
}

/**
 *Sets the project path so we can connect and extract data from it
 */
void cwProjectImageProvider::setProjectPath(QString projectPath) {
    QMutexLocker locker(&ProjectPathMutex);
    ProjectPath = projectPath;
}

/**
  Returns the project path where the images will be extracted from
  */
QString cwProjectImageProvider::projectPath() const {
    QMutexLocker locker(const_cast<QMutex*>(&ProjectPathMutex));
    return ProjectPath;
}
