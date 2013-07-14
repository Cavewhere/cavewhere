/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwUnitValue.h"

cwUnitValue::cwUnitValue(QObject *parent) :
    QObject(parent),
    Data(new PrivateData())
{
}

cwUnitValue::cwUnitValue(double value, int unit, QObject* parent) :
    QObject(parent),
    Data(new PrivateData(value, unit))
{

}

/**
  Copy constructor
  */
cwUnitValue::cwUnitValue(const cwUnitValue& other) :
    QObject(NULL),
    Data(other.Data)
{

}

/**
  Sets the unit for the length
  */
void cwUnitValue::setUnit(int unit) {
    if(Data->Unit != unit) {
        if(isUpdatingValue()) {
            //Update the value with a new value
            convertToUnit(unit);
        }

        Data->Unit = unit;
        emit unitChanged();
    }
}

void cwUnitValue::setUpdateValue(bool updateAutomatically)
{
    Data->UpdateValueWhenUnitChanged = updateAutomatically;
}

/**
  Sets the value of the length
  */
void cwUnitValue::setValue(double value) {
    if(Data->Value != value) {
        Data->Value = value;
        emit valueChanged();
    }
}

/**
  Assignment operator
  */
const cwUnitValue & cwUnitValue::operator =(const cwUnitValue &other)
{
    if(this != &other) {
        Data = other.Data;
        emit unitChanged();
        emit valueChanged();
    }
    return *this;
}
