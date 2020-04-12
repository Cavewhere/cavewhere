/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwNoteTranformation.h"
#include "cwGlobals.h"
#include "cwImageResolution.h"
#include "cwScale.h"
#include "cwLength.h"

//Qt includes
#include <QVector2D>
#include <QLineF>

//Std includes
#include "cwMath.h"

cwNoteTranformation::cwNoteTranformation(QObject* parent) :
    QObject(parent),
    North(0.0),
    Scale(new cwScale(this))
{
    connect(Scale, &cwScale::scaleChanged, this, &cwNoteTranformation::scaleChanged);
}


cwNoteTranformation::cwNoteTranformation(const cwNoteTranformation& other) :
    QObject(nullptr),
    North(other.North),
    Scale(new cwScale(*(other.Scale)))
{
    Scale->setParent(this);
}

const cwNoteTranformation& cwNoteTranformation::operator =(const cwNoteTranformation& other) {
    if(this != &other) {
        setNorthUp(other.northUp());
        *Scale = *(other.Scale);
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
  This calculates the north up based on two point in note transformation

  This returns the angle from north on the page, always assumed as up vector of (0,1)
  */
double cwNoteTranformation::calculateNorth(QPointF noteP1, QPointF noteP2) const {

    //Figure out the vector between the points
    QVector2D vectorBetweenPoints = QVector2D(noteP2) - QVector2D(noteP1);

    //Normalize it
    vectorBetweenPoints.normalize();

    //Find the angle between the north vector
    double dot = QVector2D::dotProduct(vectorBetweenPoints, QVector2D(0.0, 1.0));
    double northUp;
    if(vectorBetweenPoints.x() < 0) {
        northUp = 360.0 - acos(dot) * cwGlobals::radiansToDegrees();
    } else {
        northUp = acos(dot) * cwGlobals::radiansToDegrees();
    }

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
    if(length == nullptr) { return 0.0; }
    if(resolution == nullptr) { return 0.0; }

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
    matrix.rotate(northUp(), 0.0, 0.0, 1.0);
    matrix.scale(1.0 / scale(), 1.0 / scale(), 1.0);
    return matrix;
}

/**
Gets scaleDenominator
*/
cwLength* cwNoteTranformation::scaleDenominator() const {
    return Scale->scaleDenominator();
}

/**
 * @brief cwNoteTranformation::setScale
 * @param scale
 */
void cwNoteTranformation::setScale(double scale)
{
    Scale->setScale(scale);
}

/**
 * @brief cwNoteTranformation::scale
 * @return
 */
double cwNoteTranformation::scale() const
{
    return Scale->scale();
}

/**
Gets scaleNumerator
*/
cwLength* cwNoteTranformation::scaleNumerator() const {
    return Scale->scaleNumerator();
}
