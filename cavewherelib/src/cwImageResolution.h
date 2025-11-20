/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWIMAGERESOLUTION_H
#define CWIMAGERESOLUTION_H

//Our includes
#include "cwUnitValue.h"
class cwLength;

//Qt includes
#include <QQmlEngine>


class cwImageResolution : public cwUnitValue
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ImageResolution)

public:
    explicit cwImageResolution(QObject *parent = 0);
    cwImageResolution(double value, cwUnits::ImageResolutionUnit unit, QObject* parent = 0);

    QStringList unitNames() const;
    QString unitName(int unit) const;
    int toUnitType(QString unitName) const;
    
    Q_INVOKABLE cwImageResolution::Data convertTo(cwUnits::ImageResolutionUnit to) const;

    Q_INVOKABLE void setResolution(cwLength* length, double numberOfPixels);

protected:
    virtual void convertToUnit(int newUnit);
};


inline QStringList cwImageResolution::unitNames() const
{
    return cwUnits::imageResolutionUnitNames();
}

inline QString cwImageResolution::unitName(int unit) const
{
    return cwUnits::unitName((cwUnits::ImageResolutionUnit)unit);
}

inline int cwImageResolution::toUnitType(QString unitName) const
{
    return cwUnits::toImageResolutionUnit(unitName);
}



#endif // CWIMAGERESOLUTION_H
