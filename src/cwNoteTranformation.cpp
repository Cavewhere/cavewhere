//Our includes
#include "cwNoteTranformation.h"
#include "cwLength.h"
#include "cwGlobals.h"
#include "cwImageResolution.h"

//Qt includes
#include <QVector2D>
#include <QLineF>

//Std includes
#include "cwMath.h"

cwNoteTranformation::cwNoteTranformation(QObject* parent) :
    QObject(parent),
    North(0.0),
    ScaleNumerator(new cwLength(1, cwUnits::LengthUnitless, this)),
    ScaleDenominator(new cwLength(1, cwUnits::LengthUnitless, this))
{
    connectLengthObjects();
}


cwNoteTranformation::cwNoteTranformation(const cwNoteTranformation& other) :
    QObject(NULL),
    North(other.North),
    ScaleNumerator(new cwLength(*(other.ScaleNumerator))),
    ScaleDenominator(new cwLength(*(other.ScaleDenominator)))
{
    connectLengthObjects();
}

const cwNoteTranformation& cwNoteTranformation::operator =(const cwNoteTranformation& other) {
    if(this != &other) {
        setNorthUp(other.northUp());
        *ScaleNumerator = *(other.ScaleNumerator);
        *ScaleDenominator = *(other.ScaleDenominator);
    }
    return *this;
}

/**
  In degrees, the rotation of the page of notes such that north is aligned with the y axis.
  */
void cwNoteTranformation::setNorthUp(double degrees) {
    if(North != degrees) {
        North = degrees;
        emit northUpChanged();
    }
}

/**
  \brief Sets the scale of the notes

  This should be 1:500 where 1 unit on the page of notes equals to 500 units in cave.

  For example, 1cm on the page of notes equals to 5m in the cave.
  */
double cwNoteTranformation::scale() const {
    double numerator = scaleNumerator()->convertTo(cwUnits::Meters).value();
    double denominator = scaleDenominator()->convertTo(cwUnits::Meters).value();
    return   numerator / denominator;
}

/**
  This calculates the north up based on two point in note transformation

  This returns the angle from north on the page, always assumed as up vector of (0,1)
  */
double cwNoteTranformation::calculateNorth(QPointF noteP1, QPointF noteP2) const {
    //Figure out the vector between the points
    QVector2D vectorBetweenPoints = QVector2D(noteP2) - QVector2D(noteP1);

    double offset = 0.0;
    if(noteP1.x() < noteP1.y()) {
        offset = 360.0;
    }

    //Normalize it
    vectorBetweenPoints.normalize();

    //Find the angle between the north vector
    double dot = QVector2D::dotProduct(vectorBetweenPoints, QVector2D(0.0, 1.0));
    double northUp = offset - acos(dot) * cwGlobals::RadiansToDegrees;

    return northUp;
}

/**
  This function calculates and returns a scale.  The p1 and p2 are two points in the image.  ImageSize is the size
  of the image and dots per meter is the resolution of the image.  The scale is returned as a double.

  p1 and p2 should be in normalized note coordinates
  */
double cwNoteTranformation::calculateScale(QPointF p1, QPointF p2,
                                           cwLength* length,
                                           QSize imageSize,
                                           cwImageResolution* resolution)
{
    if(length == NULL) { return 0.0; }
    if(resolution == NULL) { return 0.0; }

    QLineF line(p1, p2);
    double a = line.dx() * imageSize.width(); //a is in dots
    double b = line.dy() * imageSize.height(); //b is in dots

    //Use Pythagorean
    double lengthInDots = sqrt(a * a + b * b);

    //Dots per meter
    cwImageResolution meterResolution = resolution->convertTo(cwUnits::DotsPerMeter);
    double dotsPerMeter = meterResolution.value();

    //Compute the scale
    double lengthInMetersOnPage = lengthInDots / dotsPerMeter;
    double lengthInMetersInCave = length->convertTo(cwUnits::Meters).value();
    double scale = lengthInMetersOnPage / lengthInMetersInCave;
    return scale;
}

/**
  \brief Get's the matrix that converts

*/
QMatrix4x4 cwNoteTranformation::matrix() const {
    QMatrix4x4 matrix;
    matrix.rotate(-northUp(), 0.0, 0.0, 1.0);
    matrix.scale(1.0 / scale(), 1.0 / scale(), 1.0);
    return matrix;
}

/**
  This connects the length objects when the scale has changed.
  */
void cwNoteTranformation::connectLengthObjects() {
    connect(ScaleNumerator, SIGNAL(valueChanged()), SIGNAL(scaleChanged()));
    connect(ScaleNumerator, SIGNAL(unitChanged()), SIGNAL(scaleChanged()));
    connect(ScaleDenominator, SIGNAL(valueChanged()), SIGNAL(scaleChanged()));
    connect(ScaleDenominator, SIGNAL(unitChanged()), SIGNAL(scaleChanged()));
}

/**
  \brief This sets the scale for the tranformation

  This will always set the numerator to 1, but keep the numerator and denominator's
  units the same.

  */
void cwNoteTranformation::setScale(double newScale)
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

    emit scaleChanged();
}
