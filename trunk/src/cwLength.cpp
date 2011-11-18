//Our includes
#include "cwLength.h"

//Qt includes
#include <QDebug>

cwLength::cwLength(QObject *parent) :
    QObject(parent),
    Data(new PrivateData())
{
}

cwLength::cwLength(double value, Unit unit, QObject* parent) :
    QObject(parent),
    Data(new PrivateData(value, unit))
{

}

/**
  Copy constructor
  */
cwLength::cwLength(const cwLength& other) :
    QObject(NULL),
    Data(new PrivateData(*(other.Data)))
{

}

/**
  Sets the unit for the length
  */
void cwLength::setUnit(Unit unit) {
    if(Data->LengthUnit != unit) {
//        //Update the value with a new value
//        double newValue = convert(value(), Data->LengthUnit, unit);
//        setValue(newValue);

        Data->LengthUnit = unit;
        emit unitChanged();
    }
}

/**
  Returns all the unit names for the length object
  */
QStringList cwLength::unitNames() {
    QStringList units;
    for(int i = in; i <= Unitless; i++) {
        units.append(unitName((Unit)i));
    }
    return units;
}

/**
  Converts the unit into a string
  */
QString cwLength::unitName(cwLength::Unit unit) {
    switch(unit) {
    case in:
        return "in";
    case ft:
        return "ft";
    case m:
        return "m";
    case mm:
        return "mm";
    case cm:
        return "cm";
    case km:
        return "km";
    case pixels:
        return "pixel";
    case Unitless:
        return "none";
    default:
        return "error";
    }
}

/**
  Sets the value of the length
  */
void cwLength::setValue(double value) {
    if(Data->Value != value) {
        Data->Value = value;
        emit valueChanged();
    }
}

/**
  This converts the value from one length unit to another
  */
double cwLength::convert(double value, Unit from, Unit to) {
    if(from == Unitless || to == Unitless) {
        return value;
    }

    double fromFactor = toMeters(from);
    double toFactor = toMeters(to);

    return value * fromFactor / toFactor;
}

/**
  This convert the length to a new length and returns a new
  cwLength object with that value
  */
cwLength cwLength::convertTo(Unit to) const {
    double convertedValue = convert(value(), unit(), to);
    return cwLength(convertedValue, to);
}

/**
  Converts the unit to unit per meter
  */
double cwLength::toMeters(cwLength::Unit unit) {
    switch(unit) {
    case in:
        return 0.0254;
    case ft:
        return 0.3048;
    case m:
        return 1.0;
    case mm:
        return 0.001;
    case cm:
        return 0.01;
    case km:
        return 1000;
    default:
        return 1.0;
    }
}

/**
  Assignment operator
  */
const cwLength & cwLength::operator =(const cwLength &other)
{
    if(this != &other) {
        Data = other.Data;
    }
    return *this;
}
