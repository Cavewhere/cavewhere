#pragma once

// Base
#include "cwAbstractNoteTransformation.h"
#include "cwNoteLiDARTransformationData.h"

// Qt
#include <QQuaternion>
#include <QProperty>
#include <QQmlEngine>

/**
 * @brief 3-D variant for LiDAR notes. Adds "UpRotation" mode and an
 *        explicit quaternion that can be bound from QML.
 *
 * Matrix order: Up rotation (quaternion) -> Z-rotate by northUp -> uniform scale.
 */
class CAVEWHERE_LIB_EXPORT cwNoteLiDARTransformation : public cwAbstractNoteTransformation
{
    Q_OBJECT
    QML_NAMED_ELEMENT(NoteLiDARTransformation)

    Q_PROPERTY(QQuaternion upRotation READ upRotation WRITE setUpRotation NOTIFY upRotationChanged  BINDABLE bindableUpRotation)
    Q_PROPERTY(UpMode upMode READ upMode WRITE setUpMode NOTIFY upModeChanged BINDABLE bindableUpMode)
    Q_PROPERTY(float upSign READ upSign WRITE setUpSign NOTIFY upSignChanged BINDABLE bindableUpSign)


public:
    //BEFORE EDITING: make sure you match with cwNoteLidarTransformationData
    enum class UpMode : int {
        Custom,
        XisUp,
        YisUp, //This is the default direction for PolyCam and Scaniverse
        ZisUp
    };
    Q_ENUM(UpMode)

    explicit cwNoteLiDARTransformation(QObject* parent = nullptr);

    // Property
    QQuaternion upRotation() const { return m_upRotation; }
    void setUpRotation(const QQuaternion& q) { m_upRotation = q; }
    QBindable<QQuaternion> bindableUpRotation() { return QBindable<QQuaternion>(&m_upRotation); }

    cwNoteLiDARTransformation::UpMode upMode() const { return m_upMode.value(); }
    void setUpMode(const cwNoteLiDARTransformation::UpMode& upMode) { m_upMode = upMode; }
    QBindable<cwNoteLiDARTransformation::UpMode> bindableUpMode() { return &m_upMode; }

    float upSign() const { return m_upSign.value(); }
    void setUpSign(float s) { m_upSign = s; }
    QBindable<float> bindableUpSign() { return &m_upSign; }

    // Transform
    QMatrix4x4 matrix() const override;

    void setData(const cwNoteLiDARTransformationData& data);
    cwNoteLiDARTransformationData data() const;

signals:
    void upRotationChanged();
    void upModeChanged();
    void upSignChanged();

private:
    QQuaternion upQuaternion() const;

    Q_OBJECT_BINDABLE_PROPERTY(cwNoteLiDARTransformation, QQuaternion, m_upRotation, &cwNoteLiDARTransformation::upRotationChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwNoteLiDARTransformation, cwNoteLiDARTransformation::UpMode, m_upMode, UpMode::YisUp, &cwNoteLiDARTransformation::upModeChanged);
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwNoteLiDARTransformation, float, m_upSign, 1.0f, &cwNoteLiDARTransformation::upSignChanged)

    //Private property
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwNoteLiDARTransformation, QQuaternion, m_rotation, QQuaternion());
};
