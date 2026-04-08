//Our includes
#include "cwVisionHeadTracker.h"

//Qt includes
#include <QCoreApplication>
#include <QPermissions>
#include <QDebug>
#include <QLabel>
#include <QPainter>

//Apple frameworks
#import <AVFoundation/AVFoundation.h>
#import <Vision/Vision.h>

// Average inter-pupillary distance in meters (used for depth estimation)
static constexpr double kAverageIPD = 0.063;

static const QStringList s_pointLabels = {
    "Nose", "L-Pupil", "R-Pupil"
};

// ─── Objective-C implementation ───────────────────────────────────────

@interface CWVisionTrackerImpl : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>

@property (nonatomic, assign) cwVisionHeadTracker* tracker;
@property (nonatomic, strong) AVCaptureSession* captureSession;
@property (nonatomic, strong) VNSequenceRequestHandler* sequenceHandler;
@property (nonatomic, strong) dispatch_queue_t videoQueue;
@property (nonatomic, assign) int frameWidth;
@property (nonatomic, assign) int frameHeight;
@property (nonatomic, assign) double focalLength;

- (void)startWithTracker:(cwVisionHeadTracker*)tracker;
- (void)stop;

@end

@implementation CWVisionTrackerImpl

- (void)startWithTracker:(cwVisionHeadTracker*)tracker
{
    self.tracker = tracker;

    self.captureSession = [[AVCaptureSession alloc] init];
    self.captureSession.sessionPreset = AVCaptureSessionPreset640x480;

    AVCaptureDevice* camera = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    if (!camera)
    {
        emit self.tracker->errorOccurred(QStringLiteral("No camera found"));
        self.tracker->stop();
        return;
    }

    NSError* error = nil;
    AVCaptureDeviceInput* input = [AVCaptureDeviceInput deviceInputWithDevice:camera error:&error];
    if (!input)
    {
        emit self.tracker->errorOccurred(
            QStringLiteral("Camera input error: %1").arg(QString::fromNSString(error.localizedDescription)));
        self.tracker->stop();
        return;
    }

    [self.captureSession addInput:input];

    AVCaptureVideoDataOutput* videoOutput = [[AVCaptureVideoDataOutput alloc] init];
    videoOutput.alwaysDiscardsLateVideoFrames = YES;
    videoOutput.videoSettings = @{
        (NSString*)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA)
    };

    self.videoQueue = dispatch_queue_create("com.cavewhere.visionTracker", DISPATCH_QUEUE_SERIAL);
    [videoOutput setSampleBufferDelegate:self queue:self.videoQueue];
    [self.captureSession addOutput:videoOutput];

    self.sequenceHandler = [[VNSequenceRequestHandler alloc] init];

    // Get actual frame dimensions
    CMFormatDescriptionRef formatDesc = camera.activeFormat.formatDescription;
    CMVideoDimensions dims = CMVideoFormatDescriptionGetDimensions(formatDesc);
    self.frameWidth = dims.width;
    self.frameHeight = dims.height;

    // Approximate focal length (typical webcam ~60 degree HFOV)
    self.focalLength = self.frameWidth;

    [self.captureSession startRunning];
}

- (void)stop
{
    if (self.captureSession.isRunning)
    {
        [self.captureSession stopRunning];
    }
    self.captureSession = nil;
    self.sequenceHandler = nil;
    self.tracker = nil;
}

- (QImage)convertPixelBufferToQImage:(CVPixelBufferRef)pixelBuffer
{
    CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);

    void* baseAddress = CVPixelBufferGetBaseAddress(pixelBuffer);
    size_t bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer);
    size_t width = CVPixelBufferGetWidth(pixelBuffer);
    size_t height = CVPixelBufferGetHeight(pixelBuffer);

    // BGRA format → QImage::Format_ARGB32 (which is BGRA in memory on little-endian)
    QImage image(static_cast<const uchar*>(baseAddress),
                 static_cast<int>(width),
                 static_cast<int>(height),
                 static_cast<int>(bytesPerRow),
                 QImage::Format_ARGB32);

    // Deep copy before unlocking
    QImage copy = image.copy();

    CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);

    // Mirror horizontally so it looks like a mirror to the user
    return copy.mirrored(true, false);
}

- (void)captureOutput:(AVCaptureOutput*)output
  didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
         fromConnection:(AVCaptureConnection*)connection
{
    CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!pixelBuffer) return;

    VNDetectFaceLandmarksRequest* landmarkRequest =
        [[VNDetectFaceLandmarksRequest alloc] initWithCompletionHandler:nil];

    if (@available(macOS 10.15, *))
    {
        landmarkRequest.revision = VNDetectFaceLandmarksRequestRevision3;
        landmarkRequest.constellation = VNRequestFaceLandmarksConstellation76Points;
    }

    NSError* error = nil;
    [self.sequenceHandler performRequests:@[landmarkRequest]
                         onCVPixelBuffer:pixelBuffer
                                   error:&error];

    if (error) return;

    NSArray<VNFaceObservation*>* results = landmarkRequest.results;
    if (results.count == 0)
    {
        cwVisionHeadTracker* tracker = self.tracker;
        if (tracker)
        {
            // Send debug frame even when no face detected
            bool wantDebug = tracker->debugPreview();
            QImage debugFrame;
            if (wantDebug)
            {
                debugFrame = [self convertPixelBufferToQImage:pixelBuffer];
            }

            QMetaObject::invokeMethod(tracker, [tracker, debugFrame, wantDebug]() {
                tracker->handleFaceLost();
                if (wantDebug)
                {
                    tracker->handleDebugFrame(debugFrame);
                }
            }, Qt::QueuedConnection);
        }
        return;
    }

    // Use the largest face
    VNFaceObservation* face = results[0];
    for (VNFaceObservation* f in results)
    {
        if (f.boundingBox.size.width * f.boundingBox.size.height >
            face.boundingBox.size.width * face.boundingBox.size.height)
        {
            face = f;
        }
    }

    VNFaceLandmarks2D* landmarks = face.landmarks;
    if (!landmarks) return;

    CGSize imageSize = CGSizeMake(self.frameWidth, self.frameHeight);

    // Extract nose tip and pupil positions for geometric head position estimation
    struct Point2D { float x; float y; };
    std::vector<Point2D> debugPoints;

    // Nose tip: last point of noseCrest
    VNFaceLandmarkRegion2D* noseCrest = landmarks.noseCrest;
    if (!noseCrest || noseCrest.pointCount == 0) return;
    const CGPoint* nosePts = [noseCrest pointsInImageOfSize:imageSize];
    Point2D noseTip = {
        static_cast<float>(nosePts[noseCrest.pointCount - 1].x),
        static_cast<float>(self.frameHeight - nosePts[noseCrest.pointCount - 1].y)
    };
    debugPoints.push_back(noseTip);

    // Left pupil
    VNFaceLandmarkRegion2D* leftPupil = landmarks.leftPupil;
    if (!leftPupil || leftPupil.pointCount == 0) return;
    const CGPoint* lpPts = [leftPupil pointsInImageOfSize:imageSize];
    Point2D leftPupilPt = {
        static_cast<float>(lpPts[0].x),
        static_cast<float>(self.frameHeight - lpPts[0].y)
    };
    debugPoints.push_back(leftPupilPt);

    // Right pupil
    VNFaceLandmarkRegion2D* rightPupil = landmarks.rightPupil;
    if (!rightPupil || rightPupil.pointCount == 0) return;
    const CGPoint* rpPts = [rightPupil pointsInImageOfSize:imageSize];
    Point2D rightPupilPt = {
        static_cast<float>(rpPts[0].x),
        static_cast<float>(self.frameHeight - rpPts[0].y)
    };
    debugPoints.push_back(rightPupilPt);

    // Compute head position from geometry:
    // Distance: estimated from inter-pupillary distance
    double ipdPixels = std::hypot(rightPupilPt.x - leftPupilPt.x,
                                   rightPupilPt.y - leftPupilPt.y);
    if (ipdPixels < 1.0) return;

    double distance = (kAverageIPD * self.focalLength) / ipdPixels;

    // X/Y: nose tip offset from image center, projected to meters at estimated distance
    double cx = self.frameWidth / 2.0;
    double cy = self.frameHeight / 2.0;
    double headX = (noseTip.x - cx) * distance / self.focalLength;
    double headY = -(noseTip.y - cy) * distance / self.focalLength; // flip Y: down→up

    QVector3D eyePos(
        static_cast<float>(headX),
        static_cast<float>(headY),
        static_cast<float>(distance)
    );

    QQuaternion headRotation; // identity — no rotation estimation needed

    // Build debug frame if enabled
    cwVisionHeadTracker* tracker = self.tracker;
    bool wantDebug = tracker && tracker->debugPreview();
    QImage debugFrame;
    if (wantDebug)
    {
        debugFrame = [self convertPixelBufferToQImage:pixelBuffer];

        // Draw landmarks on the debug frame (mirror x since image is mirrored)
        QPainter painter(&debugFrame);
        painter.setRenderHint(QPainter::Antialiasing);

        int imgW = debugFrame.width();

        for (int i = 0; i < static_cast<int>(debugPoints.size()); i++)
        {
            // Mirror x to match the mirrored image
            float drawX = imgW - debugPoints[i].x;
            float drawY = debugPoints[i].y;

            // Draw colored dot
            QColor color = (i == 0) ? Qt::red : (i < 3 ? Qt::green : Qt::blue);
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawEllipse(QPointF(drawX, drawY), 5, 5);

            // Draw label
            painter.setPen(Qt::white);
            painter.setFont(QFont("Helvetica", 10));
            painter.drawText(QPointF(drawX + 8, drawY + 4), s_pointLabels[i]);
        }

        // Draw eye position text overlay
        painter.setPen(Qt::black);
        painter.setFont(QFont("Helvetica", 40, QFont::Bold));
        painter.drawText(10, 50, QString("x: %1").arg(eyePos.x(), 0, 'f', 3));
        painter.drawText(10, 100, QString("y: %1").arg(eyePos.y(), 0, 'f', 3));
        painter.drawText(10, 150, QString("z: %1").arg(eyePos.z(), 0, 'f', 3));

        painter.end();
    }

    // Dispatch results back to the main thread
    if (tracker)
    {
        QMetaObject::invokeMethod(tracker, [tracker, eyePos, headRotation, debugFrame, wantDebug]() {
            tracker->handleFaceDetected();
            tracker->handleTrackingResult(eyePos, headRotation);
            if (wantDebug)
            {
                tracker->handleDebugFrame(debugFrame);
            }
        }, Qt::QueuedConnection);
    }
}

@end

// ─── C++ implementation ──────────────────────────────────────────────

cwVisionHeadTracker::cwVisionHeadTracker(QObject* parent)
    : cwAbstractHeadTracker(parent)
{
    m_faceLostGraceTimer.setSingleShot(true);
    m_faceLostGraceTimer.setInterval(FaceLostGracePeriodMs);
    connect(&m_faceLostGraceTimer, &QTimer::timeout,
            this, &cwVisionHeadTracker::onFaceLostTimeout);
}

cwVisionHeadTracker::~cwVisionHeadTracker()
{
    if (isRunning())
    {
        stopTracking();
    }

    delete m_debugWindow;
}

bool cwVisionHeadTracker::isAvailable() const
{
    return true;
}

void cwVisionHeadTracker::setDebugPreview(bool enabled)
{
    if (m_debugPreview == enabled) return;
    m_debugPreview = enabled;

    if (!enabled && m_debugWindow)
    {
        m_debugWindow->hide();
    }

    emit debugPreviewChanged();
}

void cwVisionHeadTracker::startTracking()
{
    requestCameraPermissionAndStart();
}

void cwVisionHeadTracker::stopTracking()
{
    m_faceLostGraceTimer.stop();

    if (m_impl)
    {
        CWVisionTrackerImpl* impl = (__bridge CWVisionTrackerImpl*)m_impl;
        [impl stop];
        CFRelease(m_impl);
        m_impl = nullptr;
    }

    if (m_debugWindow)
    {
        m_debugWindow->hide();
    }

    m_faceDetected = false;
}

void cwVisionHeadTracker::requestCameraPermissionAndStart()
{
    QCameraPermission cameraPermission;
    switch (qApp->checkPermission(cameraPermission))
    {
    case Qt::PermissionStatus::Granted:
        break;
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(cameraPermission, this, [this](const QPermission& permission) {
            if (permission.status() == Qt::PermissionStatus::Granted)
            {
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

    CWVisionTrackerImpl* impl = [[CWVisionTrackerImpl alloc] init];
    m_impl = (void*)CFBridgingRetain(impl);
    [impl startWithTracker:this];
}

void cwVisionHeadTracker::handleFaceDetected()
{
    if (!m_faceDetected)
    {
        m_faceDetected = true;
        m_faceLostGraceTimer.stop();
    }
}

void cwVisionHeadTracker::handleFaceLost()
{
    if (m_faceDetected)
    {
        m_faceDetected = false;
        m_faceLostGraceTimer.start();
    }
}

void cwVisionHeadTracker::handleTrackingResult(QVector3D eyePos, QQuaternion headRotation)
{
    setRawEyePosition(eyePos);
    setRawHeadRotation(headRotation);
}

void cwVisionHeadTracker::handleDebugFrame(QImage frame)
{
    if (!m_debugPreview) return;

    if (!m_debugWindow)
    {
        m_debugWindow = new QLabel();
        m_debugWindow->setWindowTitle("Head Tracker Debug");
        m_debugWindow->setAttribute(Qt::WA_DeleteOnClose, false);
        m_debugWindow->setMinimumSize(320, 240);
    }

    m_debugWindow->setPixmap(QPixmap::fromImage(frame).scaled(
        640, 480, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    if (!m_debugWindow->isVisible())
    {
        m_debugWindow->show();
    }
}

void cwVisionHeadTracker::onFaceLostTimeout()
{
    emit trackingLost();
    setRawEyePosition(m_defaultEyePosition);
}
