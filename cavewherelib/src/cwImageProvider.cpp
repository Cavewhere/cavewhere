/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwImageProvider.h"
#include "cwDebug.h"
#include "cwSQLManager.h"

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
#include <QJsonDocument>

//Std includes
#include <stdexcept>

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
        qDebug() << "cwImageProvider:: Couldn't convert id to a number where id=" << id;
        return QImage();
    }

    //Extract the image data from the database
    auto imageData = data(sqlId);

    //Read the image in
    QImage image = this->image(imageData);

    //Make sure the image is good
    if(image.isNull()) {
        qDebug() << "cwImageProvider:: Image isn't of format " << imageData.format();
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
    if(projectPath().isEmpty()) {
        qDebug() << "cwImageProvider: projectPath() is empty when preparing query. This will cause it to fail";
        return cwImageData();
    }

    //Needed to get around const correctness
    int connectionName = ConnectionCounter.fetchAndAddAcquire(1);

    QString request = metaDataOnly ? "requestMetadata/%1" : "resquestImage/%1";

    //Define the database
    QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", request.arg(connectionName));
    database.setDatabaseName(projectPath());

    //Create an sql connection
    bool connected = database.open();
    if(!connected) {
        qDebug() << "cwImageProvider:: Couldn't connect to database:" << ProjectPath << database.lastError().text() << LOCATION;
        return cwImageData();
    }

    cwSQLManager::instance()->beginTransaction(database, cwSQLManager::ReadOnly);

    //Setup the query
    QSqlQuery query(database);
    bool successful;
    if(metaDataOnly) {
        successful = query.prepare(requestMetadataSQL());
    } else {
        successful = query.prepare(requestImageSQL());
    }

    if(!successful) {
        qDebug() << "cwImageProvider:: Couldn't prepare query " << requestImageSQL() << "error:" << query.lastError().text();
        cwSQLManager::instance()->endTransaction(database);
        database.close();
        return cwImageData();
    }

    //Set the id that we're searching for
    query.bindValue(0, id);
    successful = query.exec();

    if(!successful) {
        qDebug() << "Couldn't exec query image id:" << id << LOCATION;
        cwSQLManager::instance()->endTransaction(database);
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
            if(QString(type) == QString(cwImageProvider::dxt1GzExtension())) {
                //Decompress the QByteArray
                imageData = qUncompress(imageData);
            }
        }

        cwSQLManager::instance()->endTransaction(database);
        database.close();
        return cwImageData(size, dotsPerMeter, type, imageData);
    }

    cwSQLManager::instance()->endTransaction(database);
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
    return image(imageData);

}

QImage cwImageProvider::image(const cwImageData &imageData) const
{
    if(imageData.format() == cwImageProvider::dxt1GzExtension()) {
        return QImage();
    }

    if(imageData.format() == cwImageProvider::croppedReferenceExtension()) {
        QJsonParseError error;
        auto document = QJsonDocument::fromJson(imageData.data(), &error);
        auto map = document.toVariant().toMap();

        auto getValue = [map](const QString& name) {
            bool okay;
            int value = map[name].toInt(&okay);
            if(!okay) {
                throw std::runtime_error(QString("Couldn't convert %1 to an int").arg(name).toStdString());
            }
            return value;
        };

        try {
            int id = getValue(cropIdKey());
            int x = getValue(cropXKey());
            int y = getValue(cropYKey());
            int width = getValue(cropWidthKey());
            int height = getValue(cropHeightKey());

            QRect cropRect(x, y, width, height);
            QImage originalImage = image(id);
            return originalImage.copy(cropRect);
        } catch(std::runtime_error error) {
            qDebug() << "Error:" << error.what();
            return QImage();
        }
    }


    return QImage::fromData(imageData.data(), imageData.format());
}

/**
 * @brief cwImageProvider::scaleTexCoords
 * @param id
 * @return This returns the
 */
QVector2D cwImageProvider::scaleTexCoords(const cwImage& image) const
{
    if(image.mipmaps().isEmpty()) {
        return QVector2D(1.0, 1.0);
    }

    QSize originalSize = image.originalSize();
    QSize firstMipmapSize = data(image.mipmaps().constFirst(), true).size();
    return QVector2D(originalSize.width() / (double)firstMipmapSize.width(),
                     originalSize.height() / (double)firstMipmapSize.height());
}

cwImageData cwImageProvider::createDxt1(QSize size, const QByteArray &uncompressData)
{
    //Compress the dxt1FileData using zlib
    auto outputData = qCompress(uncompressData, 9);

    //Add the image to the database
    return cwImageData(size, 0, cwImageProvider::dxt1GzExtension(), outputData);
}

QString cwImageProvider::imageUrl(int id)
{
    return QStringLiteral("image://") + cwImageProvider::name() + QStringLiteral("/") + QString::number(id);
}
