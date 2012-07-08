#ifndef CWIMAGERESOLUTION_H
#define CWIMAGERESOLUTION_H

//Our includes
#include "cwUnitValue.h"

class cwImageResolution : public cwUnitValue
{
    Q_OBJECT
public:
    explicit cwImageResolution(QObject *parent = 0);
    cwImageResolution(double value, cwUnits::ImageResolutionUnit unit, QObject* parent = 0);
    cwImageResolution(const cwImageResolution& other);

    QStringList unitNames();
    QString unitName(int unit);
    
    cwImageResolution convertTo(cwUnits::ImageResolutionUnit to) const;

protected:
    virtual void convertToUnit(int newUnit);
};


inline QStringList cwImageResolution::unitNames()
{
    return cwUnits::imageResolutionUnitNames();
}

inline QString cwImageResolution::unitName(int unit)
{
    return cwUnits::unitName((cwUnits::ImageResolutionUnit)unit);
}



#endif // CWIMAGERESOLUTION_H
