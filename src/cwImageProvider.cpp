//Our includes
#include "cwImageProvider.h"
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
#include <QSqlError>

const QString cwImageProvider::Name = "sqlimagequery";
const QString cwImageProvider::RequestImageSQL = "SELECT type,width,height,dotsPerMeter,imageData from Images where id=?";
const QString cwImageProvider::RequestMetadataSQL = "SELECT type,width,height,dotsPerMeter from Images where id=?";
const QByteArray cwImageProvider::Dxt1_GZ_Extension = "dxt1.gz";

QAtomicInt cwImageProvider::ConnectionCounter;

cwImageProvider::cwImageProvider() :
    QQuickImageProvider(QQuickImageProvider::Image)
{



}

/**
  \brief This extracts a image from the database

  See Qt docs for details
  */
QImage cwImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
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
QByteArray cwImageProvider::requestImageData(int id, QSize* size, QByteArray* type) {
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
void cwImageProvider::setProjectPath(QString projectPath) {
    QMutexLocker locker(&ProjectPathMutex);
    ProjectPath = projectPath;
}

/**
  Returns the project path where the images will be extracted from
  */
QString cwImageProvider::projectPath() const {
    QMutexLocker locker(const_cast<QMutex*>(&ProjectPathMutex));
    return ProjectPath;
}

/**
  Gets the metadata of the original image
  */
cwImageData cwImageProvider::originalMetadata(const cwImage &image) const {
    return data(image.original(), true); //Only get the metadata of the original
}

/**
  Gets the metadata of the image at id
  */
cwImageData cwImageProvider::data(int id, bool metaDataOnly) const {

    //Needed to get around const correctness
    cwImageProvider* castedConstThis = const_cast<cwImageProvider*>(this);
    int connectionName = castedConstThis->ConnectionCounter.fetchAndAddAcquire(1);

//    QString databaseName = QString("resquestImage/%1").arg(counter);
    QString request = metaDataOnly ? "requestMetadata/%1" : "resquestImage/%1";

    //Define the database
    QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", request.arg(connectionName));
    database.setDatabaseName(projectPath());

    //Create an sql connection
    bool connected = database.open();
    if(!connected) {
        qDebug() << "cwProjectImageProvider:: Couldn't connect to database:" << ProjectPath << database.lastError().text() << LOCATION;
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
        database.close();
        return cwImageData();
    }

    //Set the id that we're searching for
    query.bindValue(0, id);
    successful = query.exec();

    if(!successful) {
        qDebug() << "Couldn't exec query image id:" << id << LOCATION;
        database.close();
        return cwImageData();
    }

    if(query.next()) {
        QByteArray type = query.value(0).toByteArray();
        int width = query.value(1).toInt();
        int height = query.value(2).toInt();
        QSize size = QSize(width, height);
        int dotsPerMeter = query.value(3).toInt();

        QByteArray imageData;
        if(!metaDataOnly) {
            imageData = query.value(4).toByteArray();
            //Remove the zlib compression from the image
            if(QString(type) == QString(cwImageProvider::Dxt1_GZ_Extension)) {
                //Decompress the QByteArray
                imageData = qUncompress(imageData);
            }
        }

        database.close();
        return cwImageData(size, dotsPerMeter, type, imageData);
    }

    database.close();
    qDebug() << "Query has no data for id:" << id << LOCATION;
    return cwImageData();
}

/**
  \brief Gets a QImage from the image provider.  If the image at id is null, then
  this will return a empty image
  */
QImage cwImageProvider::image(int id) const
{
    cwImageData imageData = data(id);
    if(imageData.format() != cwImageProvider::Dxt1_GZ_Extension) {
        return QImage::fromData(imageData.data(), imageData.format());
    }
    return QImage();
}

///**
// * @brief cwProjectImageProvider::scaleTexCoords
// * @param id
// * @return This returns the
// */
//QVector2D cwImageProvider::scaleTexCoords(const cwImage& image) const
//{
//    if(image.mipmaps().isEmpty()) {
//        return QVector2D(1.0, 1.0);
//    }

//    QSize originalSize = data(image.original(), true).size();
//    QSize firstMipmapSize = data(image.mipmaps().first(), true).size();
//    return QVector2D(originalSize.width() / (double)firstMipmapSize.width(),
//                     originalSize.height() / (double)firstMipmapSize.height());
//}
