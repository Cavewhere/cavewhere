//Our includes
#include "cwProjectImageProvider.h"
#include "cwDebug.h"

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
const QString cwProjectImageProvider::RequestImageSQL = "SELECT type,width,height,dotsPerMeter,imageData from Images where id=?";
const QString cwProjectImageProvider::RequestMetadataSQL = "SELECT type,width,height,dotsPerMeter from Images where id=?";
const QByteArray cwProjectImageProvider::Dxt1_GZ_Extension = "dxt1.gz";

cwProjectImageProvider::cwProjectImageProvider() :
    QQmlImageProvider(QQmlImageProvider::Image)
{



}

/**
  \brief This extracts a image from the database

  See Qt docs for details
  */
QImage cwProjectImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
    bool okay;
    int sqlId = id.toInt(&okay);

    if(!okay) {
        qDebug() << "cwProjectImageProvider:: Couldn't convert id to a number where id=" << id;
        return QImage();
    }

    //Extract the image data from the database
    QByteArray type;
    QByteArray imageData = requestImageData(sqlId, size, &type);

    //Read the image in
    QImage image = QImage::fromData(imageData, type);

    //Make sure the image is good
    if(image.isNull()) {
        qDebug() << "cwProjectImageProvider:: Image isn't of format " << type;
        return QImage();
    }

    int maxSize = qMax(requestedSize.width(), requestedSize.height());
    if(maxSize > 0) {

        //Get the size of the image, scale it and return it.
        QImage scaledImage = image.scaled(QSize(maxSize, maxSize), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        *size = scaledImage.size();

        return scaledImage;
    } else {
        //Just return the image
        *size = image.size();
        return image;
    }
}

/**
  \brief Get's the image data at id.

  This will also return the size and the data.  If the image couldn't be loaded then
  this returns a empty QByteArray.

  If the format is dxt1.gz then this will unzip the data
  */
QByteArray cwProjectImageProvider::requestImageData(int id, QSize* size, QByteArray* type) {
    //Set the default size
    *size = QSize(0, 0);

    QByteArray tempTypeArray;
    if(type == NULL) {
        type = &tempTypeArray;
    }

    if(!QFileInfo(ProjectPath).isFile()) {
        qDebug() << "cwProjectImageProvider:: ProjectPath isn't set or isn't a file:" << ProjectPath << LOCATION;
        return QByteArray();
    }

    //Extra all the image data
    cwImageData imageData = data(id);

    //Set up all the data to be returned
    *size = imageData.size();
    if(type != NULL) { *type = imageData.format(); }
    return imageData.data();
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

/**
  Gets the metadata of the original image
  */
cwImageData cwProjectImageProvider::originalMetadata(const cwImage &image) const {
    return data(image.original(), true); //Only get the metadata of the original
}

/**
  Gets the metadata of the image at id
  */
cwImageData cwProjectImageProvider::data(int id, bool metaDataOnly) const {
    QString databaseName = QString("resquestImage/%1").arg(id);
    if(QSqlDatabase::contains(databaseName)) {
        QSqlDatabase::removeDatabase(databaseName);
    }

    QString request = metaDataOnly ? "requestMetadata/%1" : "resquestImage/%1";

    //Define the database
    QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", request.arg(id));
    database.setDatabaseName(projectPath());

    //Create an sql connection
    bool connected = database.open();
    if(!connected) {
        qDebug() << "cwProjectImageProvider:: Couldn't connect to database:" << ProjectPath;
        return cwImageData();
    }

    //Setup the query
    QSqlQuery query(database);
    bool successful;
    if(metaDataOnly) {
        successful = query.prepare(RequestMetadataSQL);
    } else {
        successful = query.prepare(RequestImageSQL);
    }

    if(!successful) {
        qDebug() << "cwProjectImageProvider:: Couldn't prepare query " << RequestImageSQL;
        return cwImageData();
    }

    //Set the id that we're searching for
    query.bindValue(0, id);
    query.exec();

    //Extract the data
    QSqlRecord record = query.record();
    if(record.isEmpty()) {
        qDebug() << "cwProjectImageProvider:: No data extracted for " << id;
        return cwImageData();
    }

    //Get the first row
    query.next();

    QByteArray type = query.value(0).toByteArray();
    int width = query.value(1).toInt();
    int height = query.value(2).toInt();
    QSize size = QSize(width, height);
    int dotsPerMeter = query.value(3).toInt();

    QByteArray imageData;
    if(!metaDataOnly) {
        imageData = query.value(4).toByteArray();
        //Remove the zlib compression from the image
        if(QString(type) == QString(cwProjectImageProvider::Dxt1_GZ_Extension)) {
            //Decompress the QByteArray
            imageData = qUncompress(imageData);
        }
    }

    return cwImageData(size, dotsPerMeter, type, imageData);
}

/**
  \brief Gets a QImage from the image provider.  If the image at id is null, then
  this will return a empty image
  */
QImage cwProjectImageProvider::image(int id) const
{
    cwImageData imageData = data(id);
    if(imageData.format() != cwProjectImageProvider::Dxt1_GZ_Extension) {
        return QImage::fromData(imageData.data(), imageData.format());
    }
    return QImage();
}
