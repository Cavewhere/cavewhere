/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPROJECTION_H
#define CWPROJECTION_H

//Qt includes
#include <QSharedData>
#include <QMatrix4x4>

#ifdef Q_OS_WIN32
#undef far
#undef near
#endif

/**
 * @brief The cwProjection class
 *
 * Store the data for generating the projection matri
 */
class cwProjection
{
public:

    enum Type {
        Perspective,
        PerspectiveFrustum,
        Ortho,
        Unknown
    };

    explicit cwProjection();

    void setFrustum(double left, double right, double bottom, double top, double near, double far);
    void setPerspective(double angle, double aspect, double near, double far);
    void setOrtho(double left, double right, double bottom, double top, double near, double far);

    QMatrix4x4 matrix() const;
    void setMatrix(QMatrix4x4 matrix);

    Type type() const;

    double left() const;
    double right() const;
    double bottom() const;
    double top() const;
    double far() const;
    double near() const;

    double fieldOfView() const;
    double aspectRatio() const;

    bool isNull() const;

private:

class PrivateData : public QSharedData {
public:
    PrivateData();

    QMatrix4x4 Matrix;

    cwProjection::Type Type;
    double Left;
    double Right;
    double Bottom;
    double Top;
    double Far;
    double Near;

    //For prespective only, set with perspective
    double FieldOfView;
    double AspectRatio;
};
    
    QSharedDataPointer<PrivateData> Data;


};

Q_DECLARE_METATYPE(cwProjection)


/**
 * @brief cwProjection::type
 * @return The type of projection
 *
 * If the projection is Perspective all values work, including FieldOfView and AspectRatio
 *
 * projection is Perspective, Left, Right, Bottom, Top, Far and Near are valid. FieldOfView and AspectRatio
 * wil be 0.0
 *
 * projection is Perspective, Left, Right, Bottom, Top, Far and Near are valid. FieldOfView and AspectRatio
 * wil be 0.0
 *
 * unknown all values will be zero.
 */
inline cwProjection::Type cwProjection::type() const
{
    return Data->Type;
}

/**
 * @brief cwProjection::matrix
 * @return The matrix representation of the projection
 */
inline QMatrix4x4 cwProjection::matrix() const
{
    return Data->Matrix;
}

/**
 * @brief cwProjection::left
 * @return
 */
inline double cwProjection::left() const
{
    return Data->Left;
}

/**
 * @brief cwProjection::right
 * @return
 */
inline double cwProjection::right() const
{
    return Data->Right;
}

/**
 * @brief cwProjection::bottom
 * @return
 */
inline double cwProjection::bottom() const
{
    return Data->Bottom;
}

/**
 * @brief cwProjection::top
 * @return
 */
inline double cwProjection::top() const
{
    return Data->Top;
}

/**
 * @brief cwProjection::far
 * @return
 */
inline double cwProjection::far() const
{
    return Data->Far;
}

/**
 * @brief cwProjection::near
 * @return
 */
inline double cwProjection::near() const
{
    return Data->Near;
}

/**
 * @brief cwProjection::fieldOfView
 * @return
 */
inline double cwProjection::fieldOfView() const
{
    return Data->FieldOfView;
}

/**
 * @brief cwProjection::aspectRatio
 * @return
 */
inline double cwProjection::aspectRatio() const
{
    return Data->AspectRatio;
}

/**
 * @brief cwProjection::isNull
 * @return
 */
inline bool cwProjection::isNull() const
{
    return Data->Matrix.isIdentity();
}


#endif // CWPROJECTION_H
