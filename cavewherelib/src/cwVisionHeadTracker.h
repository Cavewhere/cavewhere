#ifndef CWVISIONHEADTRACKER_H
#define CWVISIONHEADTRACKER_H

//Qt includes
#include <QTimer>
#include <QImage>

//Our includes
#include "cwAbstractHeadTracker.h"

class QLabel;

class CAVEWHERE_LIB_EXPORT cwVisionHeadTracker : public cwAbstractHeadTracker
{
    Q_OBJECT
    QML_NAMED_ELEMENT(VisionHeadTracker)

    Q_PROPERTY(bool debugPreview READ debugPreview WRITE setDebugPreview NOTIFY debugPreviewChanged)

public:
    explicit cwVisionHeadTracker(QObject* parent = nullptr);
    ~cwVisionHeadTracker() override;

    bool isAvailable() const override;

    bool debugPreview() const { return m_debugPreview; }
    void setDebugPreview(bool enabled);

    // Called from Objective-C delegate (must be public for cross-language access)
    void handleFaceDetected();
    void handleFaceLost();
    void handleTrackingResult(QVector3D eyePos, QQuaternion headRotation);
    void handleDebugFrame(QImage frame);

signals:
    void debugPreviewChanged();

protected:
    void startTracking() override;
    void stopTracking() override;

private slots:
    void onFaceLostTimeout();

private:
    void requestCameraPermissionAndStart();

    void* m_impl = nullptr; // Objective-C implementation (CWVisionTrackerImpl*)
    QTimer m_faceLostGraceTimer;
    QVector3D m_defaultEyePosition{0.0f, 0.0f, 0.0f};
    bool m_faceDetected = false;
    bool m_debugPreview = false;
    QLabel* m_debugWindow = nullptr;

    static constexpr int FaceLostGracePeriodMs = 500;
};

#endif // CWVISIONHEADTRACKER_H
