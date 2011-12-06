#ifndef CWLENGTH_H
#define CWLENGTH_H

//Our includes
#include "cwUnits.h"

//Qt includes
#include <QObject>
#include <QSharedDataPointer>
#include <QSharedData>
#include <QStringList>

class cwLength : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(cwUnits::LengthUnit unit READ unit WRITE setUnit NOTIFY unitChanged)
    Q_PROPERTY(QStringList unitNames READ unitNames NOTIFY unitNamesChanged)

//    Q_ENUMS(cwUnits::LengthUnit)
public:
    explicit cwLength(QObject *parent = 0);
    cwLength(double value, cwUnits::LengthUnit unit, QObject* parent = 0);
    cwLength(const cwLength& other);
    const cwLength& operator =(const cwLength& other);

    double value() const;
    void setValue(double value);

    cwUnits::LengthUnit unit() const;
    void setUnit(cwUnits::LengthUnit unit);

    QStringList unitNames();
    QString unitName(cwUnits::LengthUnit unit);

    cwLength convertTo(cwUnits::LengthUnit to) const;
    static double convert(double value, cwUnits::LengthUnit from, cwUnits::LengthUnit to);

signals:
    void valueChanged();
    void unitChanged();
    void unitNamesChanged();

private:

    class PrivateData : public QSharedData {
    public:
        PrivateData() :
            LengthUnit(cwUnits::Unitless),
            Value(0.0)
        {
        }

        PrivateData(double value, cwUnits::LengthUnit unit) :
            LengthUnit(unit),
            Value(value)
            {
        }

        cwUnits::LengthUnit LengthUnit;
        double Value;
    };

    QSharedDataPointer<PrivateData> Data;

};


/**
    Get's the value of the length
  */
inline double cwLength::value() const {
    return Data->Value;
}

/**
    Get's the units of the value
  */
inline cwUnits::LengthUnit cwLength::unit() const {
    return Data->LengthUnit;
}

/**
  This converts the value from one length unit to another
  */
inline double cwLength::convert(double value, cwUnits::LengthUnit from, cwUnits::LengthUnit to) {
    return cwUnits::convert(value, from, to);
}

/**
  Returns all the unit names for the length object
  */
inline QStringList cwLength::unitNames() {
    return cwUnits::lengthUnitNames();
}

/**
  Converts the unit into a string
  */
inline QString cwLength::unitName(cwUnits::LengthUnit unit) {
    return cwUnits::unitName(unit);
}

#endif // CWLENGTH_H
