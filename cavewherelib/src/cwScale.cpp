/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwScale.h"
#include "cwLength.h"

cwScale::cwScale(QObject *parent) :
    QObject(parent),
    ScaleNumerator(new cwLength(this)),
    ScaleDenominator(new cwLength(this))
{
    ScaleNumerator->setValue(1.0);
    ScaleDenominator->setValue(1.0);
    connectLengthObjects();
}

/**
  This connects the length objects when the scale has changed.
  */
void cwScale::connectLengthObjects() {
    connect(ScaleNumerator, SIGNAL(valueChanged()), SIGNAL(scaleChanged()));
    connect(ScaleNumerator, SIGNAL(unitChanged()), SIGNAL(scaleChanged()));
    connect(ScaleDenominator, SIGNAL(valueChanged()), SIGNAL(scaleChanged()));
    connect(ScaleDenominator, SIGNAL(unitChanged()), SIGNAL(scaleChanged()));
}

/**
  \brief Sets the scale of the notes

  This should be 1:500 where 1 unit on the page of notes equals to 500 units in cave.

  For example, 1cm on the page of notes equals to 5m in the cave.
  */
double cwScale::scale() const {
    double numerator = scaleNumerator()->convertTo(cwUnits::Meters).value;
    double denominator = scaleDenominator()->convertTo(cwUnits::Meters).value;
    return numerator / denominator;
}

void cwScale::setData(const Data &data)
{
    ScaleDenominator->setData(data.scaleDenominator);
    ScaleNumerator->setData(data.scaleNumerator);
}

cwScale::Data cwScale::data() const
{
    return {
        ScaleNumerator->data(),
        ScaleDenominator->data()
    };
}

/**
  \brief This sets the scale for the tranformation

  This will always set the numerator to 1, but keep the numerator and denominator's
  units the same.

  */
void cwScale::setScale(double newScale)
{
    if(newScale == scale()) { return; }

    disconnect(ScaleNumerator, SIGNAL(valueChanged()), this, SIGNAL(scaleChanged()));
    disconnect(ScaleDenominator, SIGNAL(valueChanged()), this, SIGNAL(scaleChanged()));

    scaleNumerator()->setValue(1.0);

    //Figure out the unit scaling
    double unitScale = 1.0; //scales the length for the units
    if(scaleNumerator()->unit() != cwUnits::LengthUnitless ||
            scaleDenominator()->unit() != cwUnits::LengthUnitless ) {

        unitScale = cwUnits::convert(1.0,
                                     (cwUnits::LengthUnit)scaleNumerator()->unit(),
                                     (cwUnits::LengthUnit)scaleDenominator()->unit());
    }

    double denominator = 1.0 / newScale * unitScale;
    scaleDenominator()->setValue(denominator);

    connect(ScaleNumerator, SIGNAL(valueChanged()), SIGNAL(scaleChanged()));
    connect(ScaleDenominator, SIGNAL(valueChanged()), SIGNAL(scaleChanged()));

    qDebug() << "Scale changed!";
    emit scaleChanged();
}
