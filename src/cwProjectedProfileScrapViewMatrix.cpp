//Our includes
#include "cwProjectedProfileScrapViewMatrix.h"

//std includes
#include <limits>

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

double cwProjectedProfileScrapViewMatrix::Data::absoluteAzimuth() const
{
    auto offset = [](AzimuthDirection direction) {
        switch (direction) {
        case LookingAt:
            return 0.0;
        case LeftToRight:
            return -90.0;
        case RightToLeft:
            return 90.0;
        }
        return std::numeric_limits<double>::quiet_NaN();
    };

    return azimuth() + offset(direction());
}

QMatrix4x4 cwProjectedProfileScrapViewMatrix::Data::matrix() const
{
    //Rotate the profile view
    QQuaternion profilePitch = QQuaternion::fromAxisAndAngle(1.0, 0.0, 0.0, -90.0);

    //Profile aligned with the compass direction
    QQuaternion profileYaw = QQuaternion::fromAxisAndAngle(0.0, 0.0, 1.0, absoluteAzimuth());

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

cwScrap::ScrapType cwProjectedProfileScrapViewMatrix::Data::type() const
{
    return cwScrap::ProjectedProfile;
}

/**
*
*/
cwProjectedProfileScrapViewMatrix::AzimuthDirection cwProjectedProfileScrapViewMatrix::direction() const {
    return d()->direction();
}

/**
*
*/
void cwProjectedProfileScrapViewMatrix::setDirection(cwProjectedProfileScrapViewMatrix::AzimuthDirection direction) {
    if(d()->direction() != direction) {
        d()->setDirection(direction);
        emit directionChanged();
        emit matrixChanged();
    }
}

QStringList cwProjectedProfileScrapViewMatrix::directionTypes() const {
    return {
        QString("looking at"),
        QString("left → right"), //->
        QString("left ← right") //<-
    };
}
