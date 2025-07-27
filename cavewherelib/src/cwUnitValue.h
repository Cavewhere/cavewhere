/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWUNITVALUE_H
#define CWUNITVALUE_H

//Our includes
#include "cwUnits.h"
#include "cwGlobals.h"

//Qt includes
#include <QObject>
#include <QSharedDataPointer>
#include <QSharedData>
#include <QStringList>
#include <QQmlEngine>

class CAVEWHERE_LIB_EXPORT cwUnitValue : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(UnitValue)
    QML_UNCREATABLE("UnitValue is an abstract class and can't be created directly")

    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(int unit READ unit WRITE setUnit NOTIFY unitChanged)
    Q_PROPERTY(QStringList unitNames READ unitNames NOTIFY unitNamesChanged)

public:
    struct Data {
        int unit = 0;
        double value = 0.0;
        bool updateValueWhenUnitChanged = false;
    };


    explicit cwUnitValue(QObject *parent = 0);
    cwUnitValue(double value, int unit, QObject* parent = 0);

    // [[deprecated]]
    // cwUnitValue(const cwUnitValue& other);
    // [[deprecated]]
    // const cwUnitValue& operator =(const cwUnitValue& other);
    
    double value() const;
    void setValue(double value);

    int unit() const;
    void setUnit(int unit);

    void setUpdateValue(bool updateAutomatically);
    bool isUpdatingValue() const;

    virtual QStringList unitNames() const = 0;
    virtual QString unitName(int unit) const = 0;
    Q_INVOKABLE virtual int toUnitType(QString unitName) const = 0;

    Data data() const { return d; }
    void setData(const Data& data);

signals:
    void valueChanged();
    void unitChanged();
    void unitNamesChanged();
    
public slots:

protected:
    virtual void convertToUnit(int newUnit) = 0;

    template <typename EnumType>
    cwUnitValue::Data convertToHelper(EnumType to) const {
        double newValue = cwUnits::convert(value(), (EnumType)unit(), (EnumType)to);
        return {to,
                newValue,
                isUpdatingValue()};
    }

private:
    Data d;

};

/**
    Get's the value of the length
  */
inline double cwUnitValue::value() const {
    return d.value;
}

/**
    Get's the units of the value
  */
inline int cwUnitValue::unit() const {
    return d.unit;
}

/**
 * Returns true if the value is update automatically, if units change
 * This is disabled by default
 */
inline bool cwUnitValue::isUpdatingValue() const {
   return d.updateValueWhenUnitChanged;
}

#endif // CWUNITVALUE_H
