#include "cwNoteLiDARTransformation.h"
#include "cwSurveyNetwork.h"

// Qt
#include <QVector3D>
#include <QVector2D>
#include <QtMath>

cwNoteLiDARTransformation::cwNoteLiDARTransformation(QObject* parent)
    : cwAbstractNoteTransformation(parent)
{

    m_up.setBinding([this]() {
        return upQuaternion();
    });

    m_rotation.setBinding([this]() {
        QQuaternion northUp = QQuaternion::fromAxisAndAngle(QVector3D(0.0f, 0.0f, 1.0f), m_northUp.value());
        return northUp * m_up;
    });

    // connect(this, &cwNoteLiDARTransformation::upRotationChanged, this, &cwNoteLiDARTransformation::matrixChanged);
    connect(this, &cwNoteLiDARTransformation::scaleChanged, this, &cwNoteLiDARTransformation::matrixChanged);
}


QMatrix4x4 cwNoteLiDARTransformation::matrix() const
{
    QMatrix4x4 m;

    //Apply rotation
    m.rotate(m_rotation);

    // 3) Apply uniform inverse scale (same logic as 2-D)
    const double s = scale();
    const float invS = (s == 0.0) ? 1.0f : float(1.0 / s);
    m.scale(invS, invS, invS);

    return m;
}

void cwNoteLiDARTransformation::setData(const cwNoteLiDARTransformationData &data)
{
    cwAbstractNoteTransformation::setData(data);
    m_upCustom = data.upRotation;
    m_upMode = static_cast<cwNoteLiDARTransformation::UpMode>(data.upMode);
    m_upSign = data.upSign;
}

cwNoteLiDARTransformationData cwNoteLiDARTransformation::data() const
{
   return {
       cwAbstractNoteTransformation::data(),
       m_upCustom,
        static_cast<cwNoteLiDARTransformationData::UpMode>(m_upMode.value())
    };
}

QQuaternion cwNoteLiDARTransformation::upQuaternion() const
{
    // Use only the sign (positive/negative) of upSign
    const bool neg = (m_upSign.value() < 0.0f);

    switch (m_upMode) {
    case UpMode::Custom:
        // Sign has no effect in Custom mode
        return m_upCustom;

    case UpMode::XisUp:
        // +X to +Z: rotate -90° about Y; flip sign inverts to +90°
        return QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), neg ? +90.0f : -90.0f);

    case UpMode::YisUp:
        // +Y to +Z: rotate +90° about X; flip sign inverts to -90°
        return QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), neg ? -90.0f : +90.0f);

    case UpMode::ZisUp:
        // +Z to +Z: identity; flip sign gives 180° rotation about X
        return neg ? QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), 180.0f)
                   : QQuaternion();

    default:
        return QQuaternion();
    }
}

double cwNoteLiDARTransformation::calculateNorth(const QVector3D &firstPoint, const QVector3D &secondPoint) const
{


    QVector3D delta = secondPoint - firstPoint;
    if (delta.lengthSquared() < 1e-12f) {
        return northUp();
    }

    // Rotate into the note's up-aligned space so Z is the chosen up axis
    const QVector3D rotated = upQuaternion() * delta;
    QVector2D planar(rotated.x(), rotated.y());
    if (planar.lengthSquared() < 1e-12f) {
        return northUp();
    }

    planar.normalize();
    double angle = qRadiansToDegrees(qAtan2(planar.x(), planar.y()));
    if (angle < 0.0) {
        angle += 360.0;
    }
    return angle;
}
