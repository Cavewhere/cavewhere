#include "cwProjectedProfileScrapViewMatrix.h"

/**
*
*/
cwProjectedProfileScrapViewMatrix::cwProjectedProfileScrapViewMatrix(QObject *parent) :
    cwAbstractScrapViewMatrix(parent, new cwProjectedProfileScrapViewMatrix::Data())
{

}

/**
*
*/
void cwProjectedProfileScrapViewMatrix::setAzimuth(double azimuth) {
    if(d()->azimuth() != azimuth) {
        d()->setAzimuth(azimuth);
        emit azimuthChanged();
        emit matrixChanged();
    }
}

cwProjectedProfileScrapViewMatrix *cwProjectedProfileScrapViewMatrix::clone() const
{
    return new cwProjectedProfileScrapViewMatrix(*this);
}

cwProjectedProfileScrapViewMatrix::cwProjectedProfileScrapViewMatrix(const cwProjectedProfileScrapViewMatrix &other) :
    cwAbstractScrapViewMatrix(nullptr, other.data()->clone())
{

}

/**
 *
 */
double cwProjectedProfileScrapViewMatrix::azimuth() const {
    return d()->azimuth();
}

QMatrix4x4 cwProjectedProfileScrapViewMatrix::Data::matrix() const
{
    //Rotate the profile view
    QQuaternion profilePitch = QQuaternion::fromAxisAndAngle(1.0, 0.0, 0.0, -90.0);

    //Profile aligned with the compass direction
    QQuaternion profileYaw = QQuaternion::fromAxisAndAngle(0.0, 0.0, 1.0, azimuth() - 90.0);

    //Combine the rotation to create the shot's profile view rotation
    QQuaternion profileQuat = profilePitch * profileYaw;

    QMatrix4x4 viewRotationMatrix;
    viewRotationMatrix.rotate(profileQuat);

    return viewRotationMatrix;
}

cwProjectedProfileScrapViewMatrix::Data *cwProjectedProfileScrapViewMatrix::Data::clone() const
{
    return new cwProjectedProfileScrapViewMatrix::Data(*this);
}

