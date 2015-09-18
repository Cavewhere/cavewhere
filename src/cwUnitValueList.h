/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWUNITVALUELIST_H
#define CWUNITVALUELIST_H

//Qt includes
#include <QSharedDataPointer>
#include <QMetaType>
#include <QJSValue>


//Our includes
class cwUnitValueListData;
class ValueUnit;
#include "cwUnits.h"

/**
 * @brief The cwUnitValueList class
 *
 * This does not store a list of cwUnitValue object's. This class stores a list of values and units
 * that make up a measurement. For example, "10ft 5in" could be passed to this classes constructor.
 * The constructor will create a cwUnitValueList of [10, "ft", 5, "in"]. This class allows for easy
 * conversion to a single unit using value(). This class can easily be converted to and from QVariantList
 * and QString. QVariant can automatically convert to and from this class using the following:
 *
 *  QMetaType::registerConverter(&cwUnitValueList::toString);
 *  QMetaType::registerConverter<QString, cwUnitValueList>(&cwUnitValueList::fromString);
 *  QMetaType::registerConverter(&cwUnitValueList::toList);
 *
 * The code above allows the cwUnitValueList to be converted to QVariantList, or QString when stored
 * in a QVariant. This code default is run in cwQMLRegiester.cpp
 *
 * If cwUnitValueList is invalid, this returns false.
 *
 */
class cwUnitValueList
{
public:
    cwUnitValueList();
    cwUnitValueList(const QString& listUnitAndValues);
    cwUnitValueList(const cwUnitValueList &);
    cwUnitValueList &operator=(const cwUnitValueList &);
    ~cwUnitValueList();


    bool operator !=(const cwUnitValueList& other);
    bool operator ==(const cwUnitValueList& other);

    double value(cwUnits::LengthUnit defaultFromUnit, cwUnits::LengthUnit toUnit) const;
    double value(cwUnits::ImageResolutionUnit defaultFromUnit, cwUnits::ImageResolutionUnit toUnit) const;
    double value(cwUnits::AngleUnit defaultFromUnit, cwUnits::AngleUnit toUnit) const;

    cwUnits::UnitType type() const;

    bool isValid() const;

    QString toString() const;
    QVariantList toList() const;

    static cwUnitValueList fromString(const QString& string);
    static cwUnitValueList fromList(QVariantList list);

signals:

public slots:

private:
    QSharedDataPointer<cwUnitValueListData> data;

    double value(int defaultFromUnit, int toUnit, cwUnits::UnitType unitType) const;
};

//cwUnitValueList fromString(const QString& string);

Q_DECLARE_METATYPE(cwUnitValueList)

#endif // CWUNITVALUELIST_H
