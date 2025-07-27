/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLENGTH_H
#define CWLENGTH_H

//Our includes
#include "cwUnits.h"
#include "cwUnitValue.h"
#include "cwGlobals.h"

//Qt includes
#include <QObject>
#include <QSharedDataPointer>
#include <QSharedData>
#include <QStringList>
#include <QQmlEngine>

class CAVEWHERE_LIB_EXPORT cwLength : public cwUnitValue
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Length)

public:
    explicit cwLength(QObject *parent = 0);
    cwLength(double value, cwUnits::LengthUnit unit, QObject* parent = 0);
    // cwLength(const cwLength& other);

    QStringList unitNames() const;
    QString unitName(int unit) const;
    int toUnitType(QString unitString) const;

    cwLength::Data convertTo(cwUnits::LengthUnit to) const;

protected:
    virtual void convertToUnit(int newUnit);

};


/**
  Returns all the unit names for the length object
  */
inline QStringList cwLength::unitNames() const {
    return cwUnits::lengthUnitNames();
}

/**
  Converts the unit into a string
  */
inline QString cwLength::unitName(int unit) const {
    return cwUnits::unitName((cwUnits::LengthUnit)unit);
}

/**
 * @brief cwLength::unitFromName
 * @param name - The string name of the unit
 * @return Return's the enumerate name of the unit from the name
 */
inline int cwLength::toUnitType(QString unitString) const
{
    return (int)cwUnits::toLengthUnit(unitString);
}

#endif // CWLENGTH_H
