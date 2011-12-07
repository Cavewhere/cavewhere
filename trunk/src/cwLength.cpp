//Our includes
#include "cwLength.h"

//Qt includes
#include <QDebug>

cwLength::cwLength(QObject *parent) :
    QObject(parent),
    Data(new PrivateData())
{
}

cwLength::cwLength(double value, cwUnits::LengthUnit unit, QObject* parent) :
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
void cwLength::setUnit(cwUnits::LengthUnit unit) {
    if(Data->LengthUnit != unit) {
//        //Update the value with a new value
//        double newValue = convert(value(), Data->LengthUnit, unit);
//        setValue(newValue);

        Data->LengthUnit = unit;
        emit unitChanged();
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
  This convert the length to a new length and returns a new
  cwLength object with that value
  */
cwLength cwLength::convertTo(cwUnits::LengthUnit to) const {
    double convertedValue = convert(value(), unit(), to);
    return cwLength(convertedValue, to);
}

/**
  Assignment operator
  */
const cwLength & cwLength::operator =(const cwLength &other)
{
    if(this != &other) {
        setUnit(other.unit());
        setValue(other.value());
    }
    return *this;
}
