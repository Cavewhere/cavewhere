#include "cwQuaternionRotation3d.h"

cwQuaternionRotation3d::cwQuaternionRotation3d(QObject *parent) :
    QQuickQGraphicsTransform3D(parent)
{
}

/**
Sets origin
*/
void cwQuaternionRotation3d::setOrigin(QVector3D origin) {
    if(Origin != origin) {
        Origin = origin;
        emit transformChanged();
        emit originChanged();
    }
}

/**
Sets quaternion
*/
void cwQuaternionRotation3d::setQuaternion(QQuaternion quaternion) {
    if(Quaternion != quaternion) {
        Quaternion = quaternion;
        emit transformChanged();
        emit quaternionChanged();
    }
}

void cwQuaternionRotation3d::applyTo(QMatrix4x4 *matrix) const
{
    matrix->translate(Origin);
    matrix->rotate(Quaternion);
    matrix->translate(Origin);
}

QQuickQGraphicsTransform3D *cwQuaternionRotation3d::clone(QObject *parent) const
{
    cwQuaternionRotation3d* copy = new cwQuaternionRotation3d(parent);
    copy->setOrigin(Origin);
    copy->setQuaternion(Quaternion);
    return copy;
}
