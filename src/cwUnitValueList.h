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
 * This does not store a list of cwUnitValue object's.
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
