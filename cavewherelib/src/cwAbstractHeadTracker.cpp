//Our includes
#include "cwAbstractHeadTracker.h"

cwAbstractHeadTracker::cwAbstractHeadTracker(QObject* parent)
    : QObject(parent)
{
}

cwAbstractHeadTracker::~cwAbstractHeadTracker()
{
    // Do not call stopTracking() here — it is pure virtual and the derived
    // class is already destroyed by this point. Subclasses must call stop()
    // in their own destructors if needed.
}

void cwAbstractHeadTracker::setSmoothing(double value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_smoothing, value))
    {
        return;
    }
    m_smoothing = value;
    emit smoothingChanged();
}

void cwAbstractHeadTracker::start()
{
    if (m_running)
    {
        return;
    }

    if (!isAvailable())
    {
        emit errorOccurred(QStringLiteral("Head tracker is not available"));
        return;
    }

    m_running = true;
    emit runningChanged();
    startTracking();
}

void cwAbstractHeadTracker::stop()
{
    if (!m_running)
    {
        return;
    }

    stopTracking();
    m_running = false;
    emit runningChanged();
}

void cwAbstractHeadTracker::setRawEyePosition(QVector3D position)
{
    QVector3D smoothed = applyPositionSmoothing(position);
    if ((m_eyePosition - smoothed).lengthSquared() < 1e-10f)
    {
        return;
    }
    m_eyePosition = smoothed;
    emit eyePositionChanged(m_eyePosition);
}

void cwAbstractHeadTracker::setRawHeadRotation(QQuaternion rotation)
{
    QQuaternion smoothed = applyRotationSmoothing(rotation);
    if (qFuzzyCompare(QQuaternion::dotProduct(m_headRotation, smoothed), 1.0f))
    {
        return;
    }
    m_headRotation = smoothed;
    emit headRotationChanged(m_headRotation);
}

QVector3D cwAbstractHeadTracker::applyPositionSmoothing(QVector3D raw) const
{
    if (m_eyePosition.isNull())
    {
        return raw;
    }
    float alpha = static_cast<float>(1.0 - m_smoothing);
    return m_eyePosition * (1.0f - alpha) + raw * alpha;
}

QQuaternion cwAbstractHeadTracker::applyRotationSmoothing(QQuaternion raw) const
{
    if (m_headRotation.isIdentity())
    {
        return raw;
    }
    float alpha = static_cast<float>(1.0 - m_smoothing);
    return QQuaternion::slerp(m_headRotation, raw, alpha);
}
