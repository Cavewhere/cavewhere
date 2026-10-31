#include "cwNoteTransformCalculator.h"
#include "cwDebug.h"

//Std includes
#include <cmath>

namespace {
bool isValidTransform(const cwNoteTransformCalculator::ShotTransform& transformation)
{
    const QVector3D& error = transformation.ErrorVector;
    return std::isfinite(transformation.Scale)
           && transformation.Scale > 0.0
           && std::isfinite(error.x())
           && std::isfinite(error.y())
           && std::isfinite(error.z());
}
}

cwNoteTransformCalculator::ShotTransform cwNoteTransformCalculator::averageTransformations(const QList<ShotTransform> &shotTransforms) {

    if(shotTransforms.empty()) {
        return ShotTransform();
    }

    //Values to be averaged
    QVector3D errorVectorAverage;
    double scaleAverage = 0.0;

    //Number of valid transformations
    double numberValidTransforms = 0.0;

    //Sum all the values
    foreach(ShotTransform transformation, shotTransforms) {
        if(isValidTransform(transformation)) {
            errorVectorAverage += transformation.ErrorVector;

            // cwNoteTranformation test;
            // qDebug() << "North:" << test.calculateNorth(QPointF(0.0, 0.0), transformation.ErrorVector.toPointF()) << transformation.ErrorVector;

            scaleAverage += transformation.Scale;
            numberValidTransforms += 1.0;
        }
    }

    if(numberValidTransforms == 0.0) {
        qDebug() << "No valid transfroms" << LOCATION;
        return ShotTransform();
    }

    //Do the averaging
    errorVectorAverage = errorVectorAverage / numberValidTransforms;
    scaleAverage = scaleAverage / numberValidTransforms;

    return ShotTransform(scaleAverage, errorVectorAverage);
}

cwNoteTransformationData cwNoteTransformCalculator::ShotTransform::toNoteTransform() const
{
    cwNoteTranformation transformation;
    double northAngle = transformation.calculateNorth(QPointF(0.0, 0.0), ErrorVector.toPointF());

    //    qDebug() << "ErrorVectorAverage:" << errorVectorAverage;

    transformation.setNorthUp(northAngle);
    transformation.scaleNumerator()->setValue(1);
    transformation.scaleDenominator()->setValue(Scale);

    // qDebug() << "Scale:" << Scale;

    return transformation.data();
}
