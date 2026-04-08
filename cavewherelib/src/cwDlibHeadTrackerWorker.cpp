//Our includes
#include "cwDlibHeadTrackerWorker.h"

//OpenCV includes
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>

//dlib includes
#include <dlib/image_processing.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/opencv.h>

//Qt includes
#include <QCoreApplication>
#include <QStandardPaths>
#include <QFile>
#include <QDebug>

//Std includes
#include <algorithm>

// 3D model points for a generic face (in mm, centered at nose tip).
// These correspond to specific dlib 68-point landmarks.
static const std::vector<cv::Point3d> s_modelPoints = {
    {0.0, 0.0, 0.0},          // Nose tip (landmark 30)
    {0.0, -330.0, -65.0},     // Chin (landmark 8)
    {-225.0, 170.0, -135.0},  // Left eye left corner (landmark 36)
    {225.0, 170.0, -135.0},   // Right eye right corner (landmark 45)
    {-150.0, -150.0, -125.0}, // Left mouth corner (landmark 48)
    {150.0, -150.0, -125.0},  // Right mouth corner (landmark 54)
};

// Landmark indices for the 6 key points used in PnP
static constexpr int s_landmarkIndices[] = {30, 8, 36, 45, 48, 54};

struct cwDlibHeadTrackerWorker::Impl
{
    cv::VideoCapture capture;
    dlib::frontal_face_detector faceDetector;
    dlib::shape_predictor shapePredictor;
    cv::Mat cameraMatrix;
    cv::Mat distCoeffs;
    dlib::rectangle lastFaceRect;
    bool hasLastFaceRect = false;
    int frameWidth = 0;
    int frameHeight = 0;

    // Pre-allocated per-frame buffers to avoid repeated allocation
    cv::Mat rvec;
    cv::Mat tvec;
    std::vector<cv::Point2f> imagePoints;

    bool initCamera(cwDlibHeadTrackerWorker* self)
    {
        if (!capture.open(0))
        {
            emit self->error(QStringLiteral("Failed to open camera"));
            return false;
        }

        frameWidth = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_WIDTH));
        frameHeight = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_HEIGHT));

        // Approximate intrinsic matrix based on typical webcam FOV (~60 degrees)
        double focalLength = frameWidth;
        double cx = frameWidth / 2.0;
        double cy = frameHeight / 2.0;

        cameraMatrix = (cv::Mat_<double>(3, 3) <<
            focalLength, 0, cx,
            0, focalLength, cy,
            0, 0, 1);

        distCoeffs = cv::Mat::zeros(4, 1, cv::DataType<double>::type);

        return true;
    }

    bool initDlib(cwDlibHeadTrackerWorker* self)
    {
        faceDetector = dlib::get_frontal_face_detector();

        // Look for the shape predictor model in standard locations
        QStringList searchPaths;
        searchPaths << QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources/models/shape_predictor_68_face_landmarks.dat");
        searchPaths << QCoreApplication::applicationDirPath() + QStringLiteral("/models/shape_predictor_68_face_landmarks.dat");
        searchPaths << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/models/shape_predictor_68_face_landmarks.dat");

        QString modelPath;
        for (const auto& path : searchPaths)
        {
            if (QFile::exists(path))
            {
                modelPath = path;
                break;
            }
        }

        if (modelPath.isEmpty())
        {
            emit self->error(QStringLiteral("Could not find shape_predictor_68_face_landmarks.dat model file"));
            return false;
        }

        try
        {
            dlib::deserialize(modelPath.toStdString()) >> shapePredictor;
        }
        catch (const std::exception& e)
        {
            emit self->error(QStringLiteral("Failed to load shape predictor model: %1").arg(QString::fromStdString(e.what())));
            return false;
        }

        return true;
    }

    QVector3D estimateHeadPose(const std::vector<cv::Point2f>& imagePoints,
                               cv::Mat& rvec, cv::Mat& tvec)
    {
        cv::solvePnP(s_modelPoints, imagePoints,
                     cameraMatrix, distCoeffs,
                     rvec, tvec);

        // tvec is in mm (matching the 3D model points), convert to meters
        double tx = tvec.at<double>(0) / 1000.0;
        double ty = tvec.at<double>(1) / 1000.0;
        double tz = tvec.at<double>(2) / 1000.0;

        // Map from OpenCV camera coordinates to screen-relative coordinates:
        // OpenCV: x=right, y=down, z=away from camera
        // CaveWhere: x=right, y=up, z=distance from screen (positive)
        return QVector3D(
            static_cast<float>(tx),   // x: right is positive (same)
            static_cast<float>(-ty),  // y: flip from down to up
            static_cast<float>(tz)    // z: distance from camera (always positive)
        );
    }
};

cwDlibHeadTrackerWorker::cwDlibHeadTrackerWorker(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
{
}

cwDlibHeadTrackerWorker::~cwDlibHeadTrackerWorker()
{
    if (m_impl->capture.isOpened())
    {
        m_impl->capture.release();
    }
}

void cwDlibHeadTrackerWorker::requestStop()
{
    m_stopRequested.storeRelaxed(1);
}

void cwDlibHeadTrackerWorker::process()
{
    if (!m_impl->initCamera(this))
    {
        return;
    }

    if (!m_impl->initDlib(this))
    {
        return;
    }

    cv::Mat frame;
    cv::Mat gray;
    bool wasFaceDetected = false;

    while (!m_stopRequested.loadRelaxed())
    {
        if (!m_impl->capture.read(frame))
        {
            emit error(QStringLiteral("Failed to read frame from camera"));
            break;
        }

        if (frame.empty())
        {
            continue;
        }

        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

        // Convert to dlib image (wraps cv::Mat, no copy)
        dlib::cv_image<unsigned char> dlibGray(gray);

        bool runFullDetection = (m_frameSkipCounter == 0);
        m_frameSkipCounter = (m_frameSkipCounter + 1) % DetectionInterval;

        // Face detection (full HOG detection or reuse last rect)
        std::vector<dlib::rectangle> faces;
        if (runFullDetection || !m_impl->hasLastFaceRect)
        {
            faces = m_impl->faceDetector(dlibGray);
        }
        else
        {
            // Reuse last known face rectangle for landmark tracking
            faces.push_back(m_impl->lastFaceRect);
        }

        if (faces.empty())
        {
            if (wasFaceDetected)
            {
                wasFaceDetected = false;
                m_impl->hasLastFaceRect = false;
                emit faceLost();
            }
            continue;
        }

        auto& faceRect = *std::max_element(faces.begin(), faces.end(),
            [](const auto& a, const auto& b) { return a.area() < b.area(); });

        m_impl->lastFaceRect = faceRect;
        m_impl->hasLastFaceRect = true;

        if (!wasFaceDetected)
        {
            wasFaceDetected = true;
            emit faceDetected();
        }

        // Extract 68 face landmarks
        dlib::full_object_detection shape = m_impl->shapePredictor(dlibGray, faceRect);

        m_impl->imagePoints.clear();
        for (int idx : s_landmarkIndices)
        {
            auto& part = shape.part(idx);
            m_impl->imagePoints.emplace_back(static_cast<float>(part.x()), static_cast<float>(part.y()));
        }

        QVector3D eyePos = m_impl->estimateHeadPose(m_impl->imagePoints, m_impl->rvec, m_impl->tvec);

        cv::Mat rotMat;
        cv::Rodrigues(m_impl->rvec, rotMat);

        float r00 = static_cast<float>(rotMat.at<double>(0, 0));
        float r01 = static_cast<float>(rotMat.at<double>(0, 1));
        float r02 = static_cast<float>(rotMat.at<double>(0, 2));
        float r10 = static_cast<float>(rotMat.at<double>(1, 0));
        float r11 = static_cast<float>(rotMat.at<double>(1, 1));
        float r12 = static_cast<float>(rotMat.at<double>(1, 2));
        float r20 = static_cast<float>(rotMat.at<double>(2, 0));
        float r21 = static_cast<float>(rotMat.at<double>(2, 1));
        float r22 = static_cast<float>(rotMat.at<double>(2, 2));

        // Flip Y axis: negate row 1 and column 1
        float rotData[9] = {
             r00, -r01,  r02,
            -r10,  r11, -r12,
             r20, -r21,  r22
        };
        QMatrix3x3 qRotMat(rotData);
        QQuaternion headRotation = QQuaternion::fromRotationMatrix(qRotMat);

        emit positionReady(eyePos, headRotation);
    }

    m_impl->capture.release();
}
