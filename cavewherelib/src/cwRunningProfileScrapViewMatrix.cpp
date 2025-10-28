//Our includes
#include "cwRunningProfileScrapViewMatrix.h"
#include "cwProjectedProfileScrapViewMatrix.h"

//Qt includes
#include <QQuaternion>

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
    if(!Valid) {
        return QMatrix4x4();
    }

    if(to() == from()) {
        return QMatrix4x4();
    }

    //Calculate the compass and clino from the shot
    QVector3D shotDirection = to() - from();
    QVector3D yAxis(0.0, 1.0, 0.0);
    QVector3D eulerAngles = QQuaternion::rotationTo(yAxis, shotDirection.normalized()).toEulerAngles();

    cwProjectedProfileScrapViewMatrix::Data view(-eulerAngles.z() - 90.0);
    return view.matrix();
}

cwRunningProfileScrapViewMatrix::Data *cwRunningProfileScrapViewMatrix::Data::clone() const
{
    return new cwRunningProfileScrapViewMatrix::Data(*this);
}

cwAbstractScrapViewMatrix::Type cwRunningProfileScrapViewMatrix::Data::type() const
{
    return Type::RunningProfile;
}
