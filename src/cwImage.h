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
class cwImagePrivateData;

//Qt includes
#include <QString>
#include <QImage>
#include <QStringList>
#include <QMetaType>
#include <QSharedData>
#include <QSharedDataPointer>

/**
  \brief This class stores id's to all the images in the cache database

  It stores the image relative path (to project file) for the original in the database,
  all the mipmap levels, and a icon version that's less than 512 by 512 pixels
  */
class cwImage {
public:
    cwImage();
    cwImage(const cwImage& image);
    cwImage& operator=(const cwImage& image);
    ~cwImage();

    void setMipmaps(QList<int> mipmapFiles);
    QList<int> mipmaps() const;

    int numberOfMipmapLevels() const;

    void setIcon(int icon);
    int icon() const;

    void setOriginal(int originalId);
    int original() const;

    void setOriginalFilename(QString relativePath);
    QString originalFilename() const;

    void setOriginalChecksum(QByteArray sha1);
    QByteArray originalChecksum() const;

    void setOriginalSize(QSize size);
    QSize origianlSize() const;

    void setOriginalDotsPerMeter(int dotsPerMeter);
    int originalDotsPerMeter() const;

    bool operator ==(const cwImage& other) const;
    bool operator !=(const cwImage& other) const;

    bool isValid() const;
    bool iconIsValid() const;

private:


    QSharedDataPointer<cwImagePrivateData> Data;
};



Q_DECLARE_METATYPE(cwImage)






#endif // CWIMAGE_H
