/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWIMAGEPROPERTIES_H
#define CWIMAGEPROPERTIES_H

//Qt includes
#include <QObject>
#include <QSize>
#include <QQmlEngine>

//Our includes
#include "cwImage.h"

class cwImageProperties : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ImageProperties)

    Q_PROPERTY(QSize size READ size NOTIFY sizeChanged)
    Q_PROPERTY(int dotsPerMeter READ dotsPerMeter NOTIFY dotsPerMeterChanged)

public:
    explicit cwImageProperties(QObject *parent = 0);
    
    void setImage(cwImage image);

    QSize size() const;
    int dotsPerMeter() const;

signals:
    void sizeChanged();
    void dotsPerMeterChanged();

private:
    cwImage Image;
    
};

/**
Gets size
*/
inline QSize cwImageProperties::size() const {
    return Image.originalSize();
}

/**
  Gets dotsPerMeter
  */
inline int cwImageProperties::dotsPerMeter() const {
    return Image.originalDotsPerMeter();
}
#endif // CWIMAGEPROPERTIES_H
