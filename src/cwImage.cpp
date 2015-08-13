/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwImage.h"
#include "cwGunZipReader.h"
#include "cwProject.h"

//Qt includes
#include <QDir>
#include <QDebug>
#include <QVector2D>
#include <QVector>

class cwImagePrivateData : public QSharedData {
public:
    cwImagePrivateData();

    int IconId;
    int OriginalId;
    QSize OriginalSize;
    int OriginalDotsPerMeter;
    QString OriginalRelativePath;
    QByteArray OriginalChecksum; //SHA1 of the original image

    QList<int> Mipmaps;
};

cwImagePrivateData::cwImagePrivateData() {
    IconId = -1;
    OriginalId = -1;
    OriginalDotsPerMeter = 0;
}

cwImage::cwImage() :
    Data(new cwImagePrivateData())
{

}

cwImage::cwImage(const cwImage& image) :
    Data(image.Data)
{

}

cwImage &cwImage::operator=(const cwImage &image)
{
    if (this != &image)
        Data.operator=(image.Data);
    return *this;
}

cwImage::~cwImage()
{

}

/**
  \brief Sets the mipmap image filenames
  */
 void cwImage::setMipmaps(QList<int> mipmapIds) {
    Data->Mipmaps = mipmapIds;
}

/**
  Gets the mipmap image filenames
  */
 QList<int> cwImage::mipmaps() const {
    return Data->Mipmaps;
}

/**
  Get's the number of mipmaps
  */
 int cwImage::numberOfMipmapLevels() {
    return Data->Mipmaps.size();
}

/**
  Sets the icon for this image
  */
 void cwImage::setIcon(int iconId) {
    Data->IconId = iconId;
}

/**
  Gets the icon for the filename
  */
 int cwImage::icon() const {
    return Data->IconId;
}


/**
  Sets the id filename
  */
 void cwImage::setOriginal(int original) {
    Data->OriginalId = original;
}

/**
  Gets the id to the original image

  This is usually -1, unless accessing an old VERSION_1 cavewhere file. VERSION_1 cavewhere
  files store the original inside of the database, where as VERSION_2 store them on the file
  system.
  */
 int cwImage::original() const {
    return Data->OriginalId;
}

/**
  Returns true if the original ids are equal
  */
 bool cwImage::operator ==(const cwImage& other) const {
    if(Data->OriginalId != -1 && other.Data->OriginalId != -1) {
        return Data->OriginalRelativePath == other.Data->OriginalRelativePath;
    }
    return Data->OriginalId == other.Data->OriginalId;
}

/**
  Returns true if the original ids aren't equal
  */
 bool cwImage::operator !=(const cwImage& other) const {
    return !operator ==(other);
}

/**
  \brief Sets the original size of the image
  */
 void cwImage::setOriginalSize(QSize size) {
    Data->OriginalSize = size;
}

/**
  \brief Gets the original size of the image
  */
 QSize cwImage::origianlSize() const {
    return Data->OriginalSize;
}

/**
  \brief Sets the origianl dots per meter
  */
 void cwImage::setOriginalDotsPerMeter(int dotsPerMeter) {
    Data->OriginalDotsPerMeter = dotsPerMeter;
}

/**
  \brief Sets the original dots per meter
  */
 int cwImage::originalDotsPerMeter() const {
    return Data->OriginalDotsPerMeter;
}

/**
  \brief Returns false if the image isn't valid and true if it is valid.
  */
 bool cwImage::isValid() const {
    return Data->OriginalId > -1;
}

/**
 * @brief cwImage::iconIsValid
 * @return returns true if the icon id is valid
 */
 bool cwImage::iconIsValid() const
{
    return Data->IconId > -1;
}

/**
 * @brief cwImage::setOriginalFilename
 * @param relativePath - The relativePath to the project file's
 */
 void cwImage::setOriginalFilename(QString relativePath)
{
    Data->OriginalRelativePath = relativePath;
}

/**
 * @brief cwImage::relativePath
 * @return relativePath of the original image to the project file
 */
 QString cwImage::relativePath() const
{
     return Data->OriginalRelativePath;
 }

 /**
  * @brief cwImage::setOriginalChecksum
  * @param sha1 - SHA1 hash of the original image
  *
  * The checksum is used to verify that the original image is the same that's on disk.
  */
 void cwImage::setOriginalChecksum(QByteArray sha1)
 {
    Data->OriginalChecksum = sha1;
 }

 /**
  * @brief cwImage::originalChecksum
  * @return sha1 of the original image.
  */
 QByteArray cwImage::originalChecksum() const
 {
     return Data->OriginalChecksum;
 }

