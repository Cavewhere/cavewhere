#ifndef CWDLIBHEADTRACKERWORKER_H
#define CWDLIBHEADTRACKERWORKER_H

//Qt includes
#include <QObject>
#include <QVector3D>
#include <QQuaternion>
#include <QAtomicInt>

#include <memory>

class cwDlibHeadTrackerWorker : public QObject
{
    Q_OBJECT

public:
    explicit cwDlibHeadTrackerWorker(QObject* parent = nullptr);
    ~cwDlibHeadTrackerWorker() override;

    void requestStop();

public slots:
    void process();

signals:
    void positionReady(QVector3D position, QQuaternion rotation);
    void error(QString message);
    void faceDetected();
    void faceLost();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    QAtomicInt m_stopRequested{0};
    int m_frameSkipCounter = 0;

    static constexpr int DetectionInterval = 5;
};

#endif // CWDLIBHEADTRACKERWORKER_H
