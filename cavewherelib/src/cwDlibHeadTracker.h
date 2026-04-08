#ifndef CWDLIBHEADTRACKER_H
#define CWDLIBHEADTRACKER_H

//Qt includes
#include <QThread>
#include <QTimer>

//Our includes
#include "cwAbstractHeadTracker.h"

class cwDlibHeadTrackerWorker;

class CAVEWHERE_LIB_EXPORT cwDlibHeadTracker : public cwAbstractHeadTracker
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DlibHeadTracker)

public:
    explicit cwDlibHeadTracker(QObject* parent = nullptr);
    ~cwDlibHeadTracker() override;

    bool isAvailable() const override;

protected:
    void startTracking() override;
    void stopTracking() override;

private slots:
    void onWorkerPositionReady(QVector3D position, QQuaternion rotation);
    void onWorkerError(QString message);
    void onFaceLostTimeout();
    void onFaceLostAnimationTick();

private:
    void requestCameraPermissionAndStart();

    QThread m_workerThread;
    cwDlibHeadTrackerWorker* m_worker = nullptr;

    // Face-lost animation
    QTimer m_faceLostGraceTimer;
    QTimer m_faceLostAnimationTimer;
    QVector3D m_defaultEyePosition{0.0f, 0.0f, 0.5f};
    bool m_faceDetected = false;

    static constexpr int FaceLostGracePeriodMs = 500;
    static constexpr int FaceLostAnimationIntervalMs = 16;
    static constexpr float FaceLostAnimationAlpha = 0.05f;
};

#endif // CWDLIBHEADTRACKER_H
