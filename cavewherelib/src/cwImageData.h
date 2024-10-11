/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWIMAGEDATA_H
#define CWIMAGEDATA_H

//Qt includes
#include <QSize>
#include <QString>
#include <QByteArray>
#include <QSharedData>

//Our includes
#include "cwGlobals.h"

/**
  This class stores

  */
class CAVEWHERE_LIB_EXPORT cwImageData
{
public:

    cwImageData();
    cwImageData(QSize size, int dotsPerMeter, const QByteArray& format, const QByteArray& image);
    cwImageData(const cwImageData& other);

    QSize size() const;
    int dotsPerMeter() const;
    QByteArray format() const;
    QByteArray data() const;

    cwImageData& operator=(const cwImageData& other);

private:
    class PrivateData : public QSharedData {
    public:
        QSize Size;
        int DotsPerMeter;
        QByteArray Format;
        QByteArray Data;
    };

    QSharedDataPointer<PrivateData> d;
};

/**
  \brief This gets the image data size
  */
inline QSize cwImageData::size() const {
    return d->Size;
}

/**
  \brief Gets the resolution of the image
  */
inline int cwImageData::dotsPerMeter() const {
    return d->DotsPerMeter;
}

/**
  \brief This gets the image data format

  This is usually a jpg, or a compress opengl format
  */
inline QByteArray cwImageData::format() const {
    return d->Format;
}

/**
  \brief This gets the image data
  */
inline QByteArray cwImageData::data() const {
    return d->Data;
}

#endif // CWIMAGEDATA_H
