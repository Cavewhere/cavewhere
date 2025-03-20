/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWIMAGE_H
#define CWIMAGE_H

//Our includes
class cwProject;
#include "cwGlobals.h"

//Qt includes
#include <QString>
#include <QImage>
#include <QStringList>
#include <QMetaType>
#include <QSharedData>
#include <QSharedDataPointer>
#include <QDebug>
#include <QQmlEngine>

/**
  \brief This class stores id's to all the images in the database

  It stores the image path to original in the database,
  all the mipmap levels, and a icon version that's less than 512 by 512 pixels
  */
class CAVEWHERE_LIB_EXPORT cwImage {
    Q_GADGET
    QML_NAMED_ELEMENT(cwImage)

    Q_PROPERTY(QSize originalSize READ originalSize WRITE setOriginalSize)
    Q_PROPERTY(int originalDotsPerMeter READ originalDotsPerMeter WRITE setOriginalDotsPerMeter)

public:
    cwImage();
    virtual ~cwImage() = default;

    void setMipmaps(QList<int> mipmapFiles);
    QList<int> mipmaps() const;

    int numberOfMipmapLevels();

    void setIcon(int icon);
    int icon() const;

    void setOriginal(int originalId);
    int original() const;

    void setOriginalSize(QSize size);
    QSize originalSize() const;

    void setOriginalDotsPerMeter(int dotsPerMeter);
    int originalDotsPerMeter() const;

    bool operator ==(const cwImage& other) const;
    bool operator !=(const cwImage& other) const;

    bool isOriginalValid() const;
    bool isMipmapsValid() const;
    bool isIconValid() const;

    QList<int> ids() const;

private:
    class PrivateData : public QSharedData {
    public:
        PrivateData();

        int IconId;
        int OriginalId;
        QSize OriginalSize;
        int OriginalDotsPerMeter;

        QList<int> Mipmaps;
    };

    static bool isIdValid(int id);

    QSharedDataPointer<PrivateData> Data;
};

Q_DECLARE_METATYPE(cwImage)

/**
  \brief Sets the mipmap image filenames
  */
inline void cwImage::setMipmaps(QList<int> mipmapIds) {
    Data->Mipmaps = mipmapIds;
}

/**
  Gets the mipmap image filenames
  */
inline QList<int> cwImage::mipmaps() const {
    return Data->Mipmaps;
}

/**
  Get's the number of mipmaps
  */
inline int cwImage::numberOfMipmapLevels() {
    return Data->Mipmaps.size();
}

/**
  Sets the icon for this image
  */
inline void cwImage::setIcon(int iconId) {
    Data->IconId = iconId;
}

/**
  Gets the icon for the filename
  */
inline int cwImage::icon() const {
    return Data->IconId;
}


/**
  Sets the id filename
  */
inline void cwImage::setOriginal(int original) {
    Data->OriginalId = original;
}

/**
  Gets the id to the original image
  */
inline int cwImage::original() const {
    return Data->OriginalId;
}

/**
  Returns true if the original ids are equal
  */


/**
  Returns true if the original ids aren't equal
  */
inline bool cwImage::operator !=(const cwImage& other) const {
    return !operator ==(other);
}

/**
  \brief Sets the original size of the image
  */
inline void cwImage::setOriginalSize(QSize size) {
    Data->OriginalSize = size;
}

/**
  \brief Gets the original size of the image
  */
inline QSize cwImage::originalSize() const {
    return Data->OriginalSize;
}

/**
  \brief Sets the origianl dots per meter
  */
inline void cwImage::setOriginalDotsPerMeter(int dotsPerMeter) {
    Data->OriginalDotsPerMeter = dotsPerMeter;
}

/**
  \brief Sets the original dots per meter
  */
inline int cwImage::originalDotsPerMeter() const {
    return Data->OriginalDotsPerMeter;
}

inline bool cwImage::isOriginalValid() const
{
    return isIdValid(original());
}

/**
 * @return returns true if the icon id is valid
 */
inline bool cwImage::isIconValid() const
{
    return isIdValid(Data->IconId);
}

inline bool cwImage::isIdValid(int id)
{
    return id > -1;
}

CAVEWHERE_LIB_EXPORT QDebug operator<<(QDebug debug, const cwImage &image);




#endif // CWIMAGE_H
