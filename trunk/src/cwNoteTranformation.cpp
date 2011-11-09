//Our includes
#include "cwNoteTranformation.h"
#include "cwLength.h"
#include "cwGlobals.h"

//Qt includes
#include <QVector2D>

//Std includes
#include <math.h>

cwNoteTranformation::cwNoteTranformation(QObject* parent) :
    QObject(parent),
    Data(new cwNoteTranformation::PrivateData()),
    ScaleNumerator(new cwLength(1, cwLength::Unitless, this)),
    ScaleDenominator(new cwLength(1, cwLength::Unitless, this))
{

}


cwNoteTranformation::cwNoteTranformation(const cwNoteTranformation& other) :
    QObject(NULL),
    Data(other.Data),
    ScaleNumerator(new cwLength(*(other.ScaleNumerator))),
    ScaleDenominator(new cwLength(*(other.ScaleDenominator)))
{

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

///**
//    Sets the resolution of the page of notes.
//  */
//void cwNoteTranformation::setPixelsPerInch(double pixelsPerInch) {
//    if(Data->PixelsPerInch != pixelsPerInch) {
//        Data->PixelsPerInch = pixelsPerInch;
//        emit pixelsPerInchChanged();
//    }
//}

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
