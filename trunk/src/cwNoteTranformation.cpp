//Our includes
#include "cwNoteTranformation.h"
#include "cwLength.h"
#include "cwGlobals.h"

//Qt includes
#include <QVector2D>
#include <QLineF>

//Std includes
#include <math.h>

cwNoteTranformation::cwNoteTranformation(QObject* parent) :
    QObject(parent),
    Data(new cwNoteTranformation::PrivateData()),
    ScaleNumerator(new cwLength(1, cwLength::Unitless, this)),
    ScaleDenominator(new cwLength(1, cwLength::Unitless, this))
{
    connectLengthObjects();
}


cwNoteTranformation::cwNoteTranformation(const cwNoteTranformation& other) :
    QObject(NULL),
    Data(other.Data),
    ScaleNumerator(new cwLength(*(other.ScaleNumerator))),
    ScaleDenominator(new cwLength(*(other.ScaleDenominator)))
{
    connectLengthObjects();
}

const cwNoteTranformation& cwNoteTranformation::operator =(const cwNoteTranformation& other) {
    if(this != &other) {
        Data = other.Data;
        *ScaleNumerator = *(other.ScaleNumerator);
        *ScaleDenominator = *(other.ScaleDenominator);
    }
    return *this;
}

/**
  In degrees, the rotation of the page of notes such that north is aligned with the y axis.
  */
void cwNoteTranformation::setNorthUp(double degrees) {
    if(Data->North != degrees) {
        Data->North = degrees;
        emit northUpChanged();
    }
}

/**
  \brief Sets the scale of the notes

  This should be 1:500 where 1 unit on the page of notes equals to 500 units in cave.

  For example, 1cm on the page of notes equals to 5m in the cave.
  */
double cwNoteTranformation::scale() const {
    double numerator = scaleNumerator()->convertTo(cwLength::m).value();
    double denominator = scaleDenominator()->convertTo(cwLength::m).value();
    return   numerator / denominator;
}

/**
  This calculates the north up based on two point in note transformation

  This returns the angle from north on the page, always assumed as up vector of (0,1)
  */
double cwNoteTranformation::calculateNorth(QPointF noteP1, QPointF noteP2) const {
    //Figure out the vector between the points
    QVector2D vectorBetweenPoints = QVector2D(noteP1) - QVector2D(noteP2);

    //Normalize it
    vectorBetweenPoints.normalize();

    //Find the angle between the north vector
    double dot = QVector2D::dotProduct(vectorBetweenPoints, QVector2D(0.0, 1.0));
    double northUp = acos(dot) * cwGlobals::RadiansToDegrees;

    return northUp;
}

/**
  This function calculates and returns a scale.  The p1 and p2 are two points in the image.  ImageSize is the size
  of the image and dots per meter is the resolution of the image.  The scale is returned as a double.

  p1 and p2 should be in normalized note coordinates
  */
double cwNoteTranformation::calculateScale(QPointF p1, QPointF p2,
                                           cwLength* length,
                                           QSize imageSize, double dotsPerMeter)
{
    QLineF line(p1, p2);
    double a = line.dx() * imageSize.width(); //a is in dots
    double b = line.dy() * imageSize.height(); //b is in dots

    //Use Pythagorean
    double lengthInDots = sqrt(a * a + b * b);

    //Compute the scale
    double lengthInMetersOnPage = lengthInDots / dotsPerMeter;
    double lengthInMetersInCave = length->convertTo(cwLength::m).value();
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
  This connects the length objects when the scale has changed.
  */
void cwNoteTranformation::connectLengthObjects() {
    connect(ScaleNumerator, SIGNAL(valueChanged()), SIGNAL(scaleChanged()));
    connect(ScaleNumerator, SIGNAL(unitChanged()), SIGNAL(scaleChanged()));
    connect(ScaleDenominator, SIGNAL(valueChanged()), SIGNAL(scaleChanged()));
    connect(ScaleDenominator, SIGNAL(unitChanged()), SIGNAL(scaleChanged()));
}
