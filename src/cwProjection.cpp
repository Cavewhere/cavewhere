//Our includes
#include "cwProjection.h"
#include "cwGlobals.h"
#include "cwMath.h"

cwProjection::cwProjection() :
    Data(new PrivateData)
{
}

/**
 * @brief cwProjection::setFrustum
 * @param left
 * @param right
 * @param bottom
 * @param top
 * @param near
 * @param far
 */
void cwProjection::setFrustum(double left, double right, double bottom, double top, double near, double far)
{
    Data->Matrix.setToIdentity();
    Data->Matrix.frustum(left, right, bottom, top, near, far);

    Data->Left = left;
    Data->Right = right;
    Data->Bottom = bottom;
    Data->Top = top;
    Data->Near = near;
    Data->Far = far;
    Data->Type = PerspectiveFrustum;

}

/**
 * @brief cwProjection::setPerspective
 * @param angle
 * @param aspect
 * @param near
 * @param far
 */
void cwProjection::setPerspective(double angle, double aspect, double near, double far)
{
    double top = near * tan(angle * cwGlobals::PI / 360.0);
    double bottom = -top;
    double left = bottom * aspect;
    double right = top * aspect;

    setFrustum(left, right, bottom, top, near, far);

    Data->AspectRatio = aspect;
    Data->FieldOfView = angle;
    Data->Type = Perspective;
}

/**
 * @brief cwProjection::setOrtho
 * @param left
 * @param right
 * @param bottom
 * @param top
 * @param near
 * @param far
 */
void cwProjection::setOrtho(double left, double right, double bottom, double top, double near, double far)
{
    Data->Matrix.setToIdentity();
    Data->Matrix.ortho(left, right, bottom, top, near, far);
    Data->Left = left;
    Data->Right = right;
    Data->Bottom = bottom;
    Data->Top = top;
    Data->Near = near;
    Data->Far = far;
    Data->Type = Ortho;
}



cwProjection::PrivateData::PrivateData() :
    Type(cwProjection::Unknown),
    Left(0.0),
    Right(0.0),
    Bottom(0.0),
    Top(0.0),
    Far(0.0),
    Near(0.0)
{

}


void cwProjection::setMatrix(QMatrix4x4 matrix)
{
    Data->Type = cwProjection::Unknown;
    Data->Matrix = matrix;
}
