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
    //Apply all of data atomically before emitting any signal. Setting unit and
    //value in separate steps (the old setUnit() then setValue()) briefly exposed
    //a (new unit, old value) pair to anything connected to unitChanged() — e.g.
    //cwNote recomputes every scrap's note transform on unitChanged(), and during
    //that window cwNote::dotPerMeter() was wrong by the unit-conversion factor
    //(39.37 for DotsPerInch<->DotsPerMeter), poisoning the scrap scale and
    //blowing up cwTriangulateTask::createPointGrid. The value is taken verbatim
    //(no conversion), matching the previous updateValue==false behavior.
    const bool unitDidChange = (d.unit != data.unit);
    const bool valueDidChange = (d.value != data.value);

    d.unit = data.unit;
    d.value = data.value;
    d.updateValueWhenUnitChanged = data.updateValueWhenUnitChanged;

    if(unitDidChange) { emit unitChanged(); }
    if(valueDidChange) { emit valueChanged(); }
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
