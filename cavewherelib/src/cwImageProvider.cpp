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
#include "cwAddImageTask.h"
#include "cwDiskCacher.h"

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
#include <QBuffer>
#include <QImageReader>

//Std includes
#include <stdexcept>

//xxhash
#include "xxhash.h" // Path to xxHash header

QAtomicInt cwImageProvider::ConnectionCounter;

cwImageProvider::cwImageProvider() :
    QQuickImageProvider(QQuickImageProvider::Image)
{

}

/**
  \brief This extracts a image from the database

  See Qt docs for details
  */
QImage cwImageProvider::requestImage(const QString &path, QSize *size, const QSize &requestedSize) {

    qDebug() << "Requesting path:" << path << ProjectPath;

    int maxSize = std::max(requestedSize.width(), requestedSize.height());

    if(maxSize > 0) {
        //Generate a smaller image than the original, this could be an icon
        auto hash = fileHash(imagePath(path));
        QString prefix = QStringLiteral("scaled-") + QString::number(requestedSize.width()) + QStringLiteral("_") + QString::number(requestedSize.height());
        auto key = imageCacheKey(path, prefix, hash);

        cwDiskCacher cacher(QFileInfo(ProjectPath).absoluteDir());
        auto entry = cacher.entry(key);
        if(entry.isEmpty()) {
            //Read the image in
            QImage image = this->image(path);

            //Get the size of the image, scale it and return it.
            QImage scaledImage = image.scaled(QSize(maxSize, maxSize), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            *size = scaledImage.size();

            //Add to the cache
            addToImageCache(ProjectPath, scaledImage, key);

            //Return the scaled image
            return scaledImage;
        } else {
            //Found the scaled image in cache
            auto image = this->image(cwImageData(QSize(), -1, imageCacheExtension().toUtf8(), entry));
            *size = image.size();
            return image;
        }
    } else {
        QImage image = this->image(path);

        //Make sure the image is good
        if(image.isNull()) {
            qDebug() << "cwImageProvider:: can't read image:" << path << LOCATION;
            return QImage();
        } else {
            //Just return the image
            *size = image.size();
            return image;
        }
    }

    //This is a bug, should have return before here
    Q_ASSERT(false);

}

QImage cwImageProvider::image(const QString &path) const
{
    QString fullPath = imagePath(path);
    // qDebug() << "FullPath:" << fullPath;

    //Extract the image data from the disk
    auto imageData = data(fullPath);

    //Read the image in
    QImage image = this->image(imageData);

    return image;
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
    QMutexLocker locker(&ProjectPathMutex);
    return ProjectPath;
}

QString cwImageProvider::imagePath(const QString &relativeImagePath) const
{
    QMutexLocker locker(&ProjectPathMutex);

    QFileInfo info(ProjectPath);
    QDir dir = info.absoluteDir();

    return dir.absoluteFilePath(relativeImagePath);
}

/**
  Gets the metadata of the original image
  */
// cwImageData cwImageProvider::originalMetadata(const cwImage &image) const {
//     return data(image.original(), true); //Only get the metadata of the original
// }

/**
  Gets the metadata of the image at id

  This is used get old data from sqlite database, this should NOT be used in new coe
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

cwImageData cwImageProvider::data(QString filename) const
{
    QFileInfo projectInfo(ProjectPath);
    auto projectDir = projectInfo.absoluteDir();
    auto imagePath = projectDir.absoluteFilePath(filename);

    if(!QFileInfo::exists(imagePath)) {
        qDebug() << "cwImageProvider can't open:" << filename << LOCATION;
    }

    QFileInfo info(filename);
    QString extention = info.suffix();

    QSize size;
    QByteArray data;

    if(extention == imageCacheExtension()) {
        //From the disk cacher
        cwDiskCacher cacher(projectDir);
        data = cacher.entry(imagePath);

        QBuffer buffer(&data);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer);
        size = reader.size();

    } else {
        //Normal file
        QFile file(imagePath);
        file.open(QFile::ReadOnly);
        data = file.readAll();
        QImageReader reader(filename);
        size = reader.size();
    }

    int dotPerMeter = -1; //Can't query this from the image, require QImage

    return cwImageData(size, dotPerMeter, extention.toUtf8(), data);
}

QString cwImageProvider::absoluteImagePath(const cwImage &image) const
{
    return imagePath(image.path());
}

// /**
//   \brief Gets a QImage from the image provider.  If the image at id is null, then
//   this will return a empty image
//   */
// QImage cwImageProvider::image(int id) const
// {
//     cwImageData imageData = data(id);
//     return image(imageData);

// }

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
            QString path = map[cropIdKey()].toString();
            int x = getValue(cropXKey());
            int y = getValue(cropYKey());
            int width = getValue(cropWidthKey());
            int height = getValue(cropHeightKey());

            QRect cropRect(x, y, width, height);
            QImage originalImage(imagePath(path));
            return originalImage.copy(cropRect);
        } catch(std::runtime_error error) {
            qDebug() << "Error:" << error.what();
            return QImage();
        }
    }

    auto imageByteData = imageData.data();
    return cwAddImageTask::imageWithAutoTransform(imageByteData, imageData.format());
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

QString cwImageProvider::imageUrl(QString relativePath)
{
    return QStringLiteral("image://") + cwImageProvider::name() + QStringLiteral("/") + relativePath;
}

quint64 cwImageProvider::imageHash(const QImage &image)
{
    return XXH3_64bits(image.constBits(), static_cast<size_t>(image.sizeInBytes()));
}

quint64 cwImageProvider::fileHash(const QString &path)
{
    QFile file(path);
    file.open(QFile::ReadOnly);
    auto byteArray = file.readAll();
    return toHash(byteArray);
}

quint64 cwImageProvider::toHash(const QByteArray &data)
{
    return XXH3_64bits(data.constData(), data.size());
}

cwDiskCacher::Key cwImageProvider::addToImageCache(
    const QString& projectFilename,
    const QImage& image,
    const cwDiskCacher::Key& key)
{
    QFileInfo info(projectFilename);

    QByteArray imageData;
    QBuffer buffer(&imageData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    buffer.close();

    //Save image data into the disk cacher
    cwDiskCacher cacher(info.dir());
    cacher.insert(key, imageData);
    return key;
}

cwDiskCacher::Key cwImageProvider::imageCacheKey(const QString &pathToImage, const QString &keyPrefix, quint64 parentImageHash)
{
    QFileInfo info(pathToImage);

    cwDiskCacher::Key key {
        keyPrefix + QStringLiteral("-") + info.fileName() + QStringLiteral(".") + cwImageProvider::imageCacheExtension(),
        info.dir(),
        QString::number(parentImageHash, 16)
    };

    return key;
}
