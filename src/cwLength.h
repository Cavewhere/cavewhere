#ifndef CWLENGTH_H
#define CWLENGTH_H

//Our includes
#include "cwUnits.h"
#include "cwUnitValue.h"

//Qt includes
#include <QObject>
#include <QSharedDataPointer>
#include <QSharedData>
#include <QStringList>

class cwLength : public cwUnitValue
{
    Q_OBJECT

public:
    explicit cwLength(QObject *parent = 0);
    cwLength(double value, cwUnits::LengthUnit unit, QObject* parent = 0);
    cwLength(const cwLength& other);

    QStringList unitNames();
    QString unitName(int unit);

    cwLength convertTo(cwUnits::LengthUnit to) const;

protected:
    virtual void convertToUnit(int newUnit);

};


/**
  Returns all the unit names for the length object
  */
inline QStringList cwLength::unitNames() {
    return cwUnits::lengthUnitNames();
}

/**
  Converts the unit into a string
  */
inline QString cwLength::unitName(int unit) {
    return cwUnits::unitName((cwUnits::LengthUnit)unit);
}

#endif // CWLENGTH_H
