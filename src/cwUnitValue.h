#ifndef CWUNITVALUE_H
#define CWUNITVALUE_H

//Our includes
#include "cwUnits.h"

//Qt includes
#include <QObject>
#include <QSharedDataPointer>
#include <QSharedData>
#include <QStringList>

class cwUnitValue : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(int unit READ unit WRITE setUnit NOTIFY unitChanged)
    Q_PROPERTY(QStringList unitNames READ unitNames NOTIFY unitNamesChanged)

public:
    explicit cwUnitValue(QObject *parent = 0);
    cwUnitValue(double value, int unit, QObject* parent = 0);
    cwUnitValue(const cwUnitValue& other);
    const cwUnitValue& operator =(const cwUnitValue& other);
    
    double value() const;
    void setValue(double value);

    int unit() const;
    void setUnit(int unit);

    virtual QStringList unitNames() = 0;
    virtual QString unitName(int unit) = 0;

signals:
    void valueChanged();
    void unitChanged();
    void unitNamesChanged();
    
public slots:

private:
    class PrivateData : public QSharedData {
    public:
        PrivateData() :
            Unit(-1),
            Value(0.0)
        {
        }

        PrivateData(double value, int unit) :
            Unit(unit),
            Value(value)
            {
        }

        int Unit;
        double Value;
    };

    QSharedDataPointer<PrivateData> Data;

    
};

/**
    Get's the value of the length
  */
inline double cwUnitValue::value() const {
    return Data->Value;
}

/**
    Get's the units of the value
  */
inline int cwUnitValue::unit() const {
    return Data->Unit;
}

#endif // CWUNITVALUE_H
