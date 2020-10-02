//Our includes
#include "cwRunningProfileScrapViewMatrix.h"
#include "cwProjectedProfileScrapViewMatrix.h"

cwRunningProfileScrapViewMatrix::cwRunningProfileScrapViewMatrix(QObject* parent) :
    cwAbstractScrapViewMatrix(parent, new Data())
{

}

cwRunningProfileScrapViewMatrix *cwRunningProfileScrapViewMatrix::clone() const
{
    return new cwRunningProfileScrapViewMatrix(*this);
}

cwRunningProfileScrapViewMatrix::cwRunningProfileScrapViewMatrix(const cwRunningProfileScrapViewMatrix &other) :
    cwAbstractScrapViewMatrix(nullptr, other.data()->clone())
{

}

QMatrix4x4 cwRunningProfileScrapViewMatrix::Data::matrix() const
{
    //Calculate the compass and clino from the shot
    QVector3D shotDirection = to() - from();
    QVector3D yAxis(0.0, 1.0, 0.0);
    QVector3D eulerAngles = QQuaternion::rotationTo(yAxis, shotDirection.normalized()).toEulerAngles();

    cwProjectedProfileScrapViewMatrix::Data view(-eulerAngles.z());
    return view.matrix();
}

cwRunningProfileScrapViewMatrix::Data *cwRunningProfileScrapViewMatrix::Data::clone() const
{
    return new cwRunningProfileScrapViewMatrix::Data(*this);
}
