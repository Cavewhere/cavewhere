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

    void setUpdateValue(bool updateAutomatically);
    bool isUpdatingValue() const;

    virtual QStringList unitNames() const = 0;
    virtual QString unitName(int unit) const = 0;
    Q_INVOKABLE virtual int toUnitType(QString unitName) const = 0;


signals:
    void valueChanged();
    void unitChanged();
    void unitNamesChanged();
    
public slots:

protected:
    virtual void convertToUnit(int newUnit) = 0;

private:
    class PrivateData : public QSharedData {
    public:
        PrivateData() :
            Unit(0),
            Value(0.0),
            UpdateValueWhenUnitChanged(false)
        {
        }

        PrivateData(double value, int unit) :
            Unit(unit),
            Value(value),
            UpdateValueWhenUnitChanged(false)
            {
        }

        int Unit;
        double Value;
        bool UpdateValueWhenUnitChanged;
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

/**
 * Returns true if the value is update automatically, if units change
 * This is disabled by default
 */
inline bool cwUnitValue::isUpdatingValue() const {
   return Data->UpdateValueWhenUnitChanged;
}

#endif // CWUNITVALUE_H
