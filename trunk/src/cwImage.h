#ifndef CWIMAGE_H
#define CWIMAGE_H

//Our includes
class cwProject;

//Qt includes
#include <QString>
#include <QImage>
#include <QStringList>
#include <QMetaType>

/**
  \brief This class stores id's to all the images in the database

  It stores the image path to original in the database,
  all the mipmap levels, and a icon version that's less than 512 by 512 pixels
  */
class cwImage {
public:
    cwImage();

    void setMipmaps(QList<int> mipmapFiles);
    QList<int> mipmaps();

    int numberOfMipmapLevels();

    void setIcon(int icon);
    int icon() const;

    void setOriginal(int originalId);
    int original() const;

    bool operator ==(const cwImage& other);
    bool operator !=(const cwImage& other);

private:
    QList<int> Mipmaps;
    int IconId;
    int OriginalId;
};

Q_DECLARE_METATYPE(cwImage)

/**
  \brief Sets the mipmap image filenames
  */
inline void cwImage::setMipmaps(QList<int> mipmapIds) {
    Mipmaps = mipmapIds;
}

/**
  Gets the mipmap image filenames
  */
inline QList<int> cwImage::mipmaps() {
    return Mipmaps;
}

/**
  Get's the number of mipmaps
  */
inline int cwImage::numberOfMipmapLevels() {
    return Mipmaps.size();
}

/**
  Sets the icon for this image
  */
inline void cwImage::setIcon(int iconId) {
    IconId = iconId;
}

/**
  Gets the icon for the filename
  */
inline int cwImage::icon() const {
    return IconId;
}


/**
  Sets the id filename
  */
inline void cwImage::setOriginal(int original) {
    OriginalId = original;
}

/**
  Gets the id to the original image
  */
inline int cwImage::original() const {
    return OriginalId;
}

/**
  Returns true if the original ids are equal
  */
inline bool cwImage::operator ==(const cwImage& other) {
    return OriginalId == other.OriginalId;
}

/**
  Returns true if the original ids aren't equal
  */
inline bool cwImage::operator !=(const cwImage& other) {
    return !operator ==(other);
}



#endif // CWIMAGE_H
