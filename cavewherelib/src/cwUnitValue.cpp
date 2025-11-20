/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwUnitValue.h"

cwUnitValue::cwUnitValue(QObject *parent) :
    QObject(parent)
{
}

cwUnitValue::cwUnitValue(double value, int unit, QObject* parent) :
    QObject(parent),
    d({unit, value})
{

}

/**
  Sets the unit for the length
  */
void cwUnitValue::setUnit(int unit) {
    if(d.unit != unit) {
        if(isUpdatingValue()) {
            //Update the value with a new value
            convertToUnit(unit);
        }

        d.unit = unit;
        emit unitChanged();
    }
}

void cwUnitValue::setUpdateValue(bool updateAutomatically)
{
    d.updateValueWhenUnitChanged = updateAutomatically;
}

void cwUnitValue::setData(const Data &data)
{
    setValue(data.value);
    setUnit(data.unit);
    setUpdateValue(data.updateValueWhenUnitChanged);
}

/**
  Sets the value of the length
  */
void cwUnitValue::setValue(double value) {
    if(d.value != value) {
        d.value = value;
        emit valueChanged();
    }
}
