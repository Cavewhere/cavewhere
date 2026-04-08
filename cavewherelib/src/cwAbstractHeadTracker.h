#ifndef CWABSTRACTHEADTRACKER_H
#define CWABSTRACTHEADTRACKER_H

//Qt includes
#include <QObject>
#include <QVector3D>
#include <QQuaternion>
#include <QQmlEngine>

//Our includes
#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwAbstractHeadTracker : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(AbstractHeadTracker)
    QML_UNCREATABLE("AbstractHeadTracker is a base class")

    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)
    Q_PROPERTY(double smoothing READ smoothing WRITE setSmoothing NOTIFY smoothingChanged)
    Q_PROPERTY(QVector3D eyePosition READ eyePosition NOTIFY eyePositionChanged)
    Q_PROPERTY(QQuaternion headRotation READ headRotation NOTIFY headRotationChanged)

public:
    explicit cwAbstractHeadTracker(QObject* parent = nullptr);
    virtual ~cwAbstractHeadTracker();

    bool isRunning() const;
    virtual bool isAvailable() const = 0;

    double smoothing() const;
    void setSmoothing(double value);

    QVector3D eyePosition() const;
    QQuaternion headRotation() const;

public slots:
    void start();
    void stop();

signals:
    void runningChanged();
    void availableChanged();
    void smoothingChanged();
    void eyePositionChanged(QVector3D position);
    void headRotationChanged(QQuaternion rotation);
    void trackingLost();
    void errorOccurred(QString message);

protected:
    void setRawEyePosition(QVector3D position);
    void setRawHeadRotation(QQuaternion rotation);

    virtual void startTracking() = 0;
    virtual void stopTracking() = 0;

private:
    QVector3D applyPositionSmoothing(QVector3D raw) const;
    QQuaternion applyRotationSmoothing(QQuaternion raw) const;

    bool m_running = false;
    double m_smoothing = 0.5;
    QVector3D m_eyePosition;
    QQuaternion m_headRotation;
};

inline bool cwAbstractHeadTracker::isRunning() const
{
    return m_running;
}

inline double cwAbstractHeadTracker::smoothing() const
{
    return m_smoothing;
}

inline QVector3D cwAbstractHeadTracker::eyePosition() const
{
    return m_eyePosition;
}

inline QQuaternion cwAbstractHeadTracker::headRotation() const
{
    return m_headRotation;
}

#endif // CWABSTRACTHEADTRACKER_H
