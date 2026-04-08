//Our includes
#include "cwDlibHeadTracker.h"
#include "cwDlibHeadTrackerWorker.h"

//Qt includes
#include <QCoreApplication>
#include <QPermissions>
#include <QDebug>

cwDlibHeadTracker::cwDlibHeadTracker(QObject* parent)
    : cwAbstractHeadTracker(parent)
{
    m_faceLostGraceTimer.setSingleShot(true);
    m_faceLostGraceTimer.setInterval(FaceLostGracePeriodMs);
    connect(&m_faceLostGraceTimer, &QTimer::timeout,
            this, &cwDlibHeadTracker::onFaceLostTimeout);

    m_faceLostAnimationTimer.setInterval(FaceLostAnimationIntervalMs);
    connect(&m_faceLostAnimationTimer, &QTimer::timeout,
            this, &cwDlibHeadTracker::onFaceLostAnimationTick);
}

cwDlibHeadTracker::~cwDlibHeadTracker()
{
    if (isRunning())
    {
        stopTracking();
    }
}

bool cwDlibHeadTracker::isAvailable() const
{
    // Basic availability check - a camera exists.
    // Full permission check happens at start time.
    return true;
}

void cwDlibHeadTracker::startTracking()
{
    requestCameraPermissionAndStart();
}

void cwDlibHeadTracker::stopTracking()
{
    m_faceLostGraceTimer.stop();
    m_faceLostAnimationTimer.stop();

    if (m_worker)
    {
        m_worker->requestStop();
        m_workerThread.quit();
        m_workerThread.wait();
        m_worker = nullptr;
    }

    m_faceDetected = false;
}

void cwDlibHeadTracker::requestCameraPermissionAndStart()
{
#ifdef Q_OS_MACOS
    QCameraPermission cameraPermission;
    switch (qApp->checkPermission(cameraPermission))
    {
    case Qt::PermissionStatus::Granted:
        break;
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(cameraPermission, this, [this](const QPermission& permission) {
            if (permission.status() == Qt::PermissionStatus::Granted)
            {
                // Permission granted, now actually start
                requestCameraPermissionAndStart();
            }
            else
            {
                emit errorOccurred(QStringLiteral("Camera permission denied"));
                stop();
            }
        });
        return;
    case Qt::PermissionStatus::Denied:
        emit errorOccurred(QStringLiteral("Camera permission denied. Enable in System Settings > Privacy & Security > Camera."));
        stop();
        return;
    }
#endif

    // Create worker and move to thread
    m_worker = new cwDlibHeadTrackerWorker();
    m_worker->moveToThread(&m_workerThread);

    connect(&m_workerThread, &QThread::finished,
            m_worker, &QObject::deleteLater);
    connect(&m_workerThread, &QThread::started,
            m_worker, &cwDlibHeadTrackerWorker::process);
    connect(m_worker, &cwDlibHeadTrackerWorker::positionReady,
            this, &cwDlibHeadTracker::onWorkerPositionReady);
    connect(m_worker, &cwDlibHeadTrackerWorker::error,
            this, &cwDlibHeadTracker::onWorkerError);
    connect(m_worker, &cwDlibHeadTrackerWorker::faceLost,
            this, [this]() {
        m_faceDetected = false;
        m_faceLostGraceTimer.start();
    });
    connect(m_worker, &cwDlibHeadTrackerWorker::faceDetected,
            this, [this]() {
        m_faceDetected = true;
        m_faceLostGraceTimer.stop();
        m_faceLostAnimationTimer.stop();
    });

    m_workerThread.setObjectName(QStringLiteral("HeadTrackerWorker"));
    m_workerThread.start();
}

void cwDlibHeadTracker::onWorkerPositionReady(QVector3D position, QQuaternion rotation)
{
    setRawEyePosition(position);
    setRawHeadRotation(rotation);
}

void cwDlibHeadTracker::onWorkerError(QString message)
{
    emit errorOccurred(message);
    stop();
}

void cwDlibHeadTracker::onFaceLostTimeout()
{
    emit trackingLost();
    // Snap back to default position immediately
    setRawEyePosition(m_defaultEyePosition);
}

void cwDlibHeadTracker::onFaceLostAnimationTick()
{
    // No longer used — kept for binary compatibility
}
