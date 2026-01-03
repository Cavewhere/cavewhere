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
#include <cmath>
#include <limits>

cwNoteTranformation::cwNoteTranformation(QObject* parent) :
    cwAbstractNoteTransformation(parent)
{
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
    if(length == nullptr || resolution == nullptr) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    QLineF line(p1, p2);
    double a = line.dx() * imageSize.width(); //a is in dots
    double b = line.dy() * imageSize.height(); //b is in dots

    //Use Pythagorean
    double lengthInDots = sqrt(a * a + b * b);
    if(!std::isfinite(lengthInDots) || lengthInDots <= 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    //Dots per meter
    cwImageResolution::Data meterResolution = resolution->convertTo(cwUnits::DotsPerMeter);
    double dotsPerMeter = meterResolution.value;
    if(!std::isfinite(dotsPerMeter) || dotsPerMeter <= 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    //Compute the scale
    double lengthInMetersOnPage = lengthInDots / dotsPerMeter;
    double lengthInMetersInCave = length->convertTo(cwUnits::Meters).value;
    if(!std::isfinite(lengthInMetersInCave) || lengthInMetersInCave <= 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    double scale = lengthInMetersOnPage / lengthInMetersInCave;
    if(!std::isfinite(scale) || scale <= 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return scale;
}

double cwNoteTranformation::calculateScaleForRendered(QPointF p1, QPointF p2,
                                                      cwLength* length,
                                                      const cwImage& image,
                                                      const QSize& renderedSize,
                                                      cwImageResolution* resolution)
{
    if(length == nullptr || resolution == nullptr) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    if(renderedSize.isEmpty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    QLineF line(p1, p2);
    const double dx = line.dx() * renderedSize.width();
    const double dy = line.dy() * renderedSize.height();
    const double lengthPixels = sqrt(dx * dx + dy * dy);
    if(!std::isfinite(lengthPixels) || lengthPixels <= 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double lengthInDots = resolution->nativePixelLength(image, renderedSize, lengthPixels);
    if(!std::isfinite(lengthInDots) || lengthInDots <= 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    //Dots per meter
    cwImageResolution::Data meterResolution = resolution->convertTo(cwUnits::DotsPerMeter);
    const double dotsPerMeter = meterResolution.value;
    if(!std::isfinite(dotsPerMeter) || dotsPerMeter <= 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    //Compute the scale
    const double lengthInMetersOnPage = lengthInDots / dotsPerMeter;
    const double lengthInMetersInCave = length->convertTo(cwUnits::Meters).value;
    if(!std::isfinite(lengthInMetersInCave) || lengthInMetersInCave <= 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double scale = lengthInMetersOnPage / lengthInMetersInCave;
    if(!std::isfinite(scale) || scale <= 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return scale;
}

/**
  \brief Get's the matrix that converts

*/
QMatrix4x4 cwNoteTranformation::matrix() const {
    double currentScale = scale();
    if(!std::isfinite(currentScale) || currentScale <= 0.0) {
        return QMatrix4x4();
    }

    QMatrix4x4 matrix;
    matrix.rotate(northUp(), 0.0, 0.0, 1.0);
    matrix.scale(1.0 / currentScale, 1.0 / currentScale, 1.0);
    return matrix;
}
