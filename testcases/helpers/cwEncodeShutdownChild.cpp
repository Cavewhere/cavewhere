/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Child process for test_cwKtx2Codec's shutdown case: starts a UASTC encode,
// waits until its waves are running on the task pool, then returns from main
// while the encode is still in flight, so ~QCoreApplication tears the pools
// down underneath it.

//Qt includes
#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFuture>
#include <QImage>
#include <QThread>
#include <QThreadPool>

//Our includes
#include "cwConcurrent.h"
#include "cwEncodeShutdownChild.h"
#include "cwKtx2Codec.h"
#include "cwTask.h"

//Std includes
#include <cstdio>

namespace {
    constexpr int kMaxChannel = 255;
    constexpr int kExitEncodeNeverStarted = 2;

    QImage gradientImage(int size)
    {
        QImage image(size, size, QImage::Format_RGBA8888);
        for(int y = 0; y < size; y++) {
            for(int x = 0; x < size; x++) {
                const int red = (x * kMaxChannel) / (size - 1);
                const int green = (y * kMaxChannel) / (size - 1);
                image.setPixelColor(x, y, QColor(red, green, kMaxChannel - red));
            }
        }
        return image;
    }

    //The lane runs only the wave loop, so a busy lane and a busy pool mean waves are queuing
    bool encodeIsInFlight()
    {
        return cw::ktx2::encodeLane()->activeThreadCount() > 0
               && cwTask::threadPool()->activeThreadCount() > 0;
    }

    bool waitForEncodeInFlight()
    {
        QElapsedTimer timer;
        timer.start();
        while(!encodeIsInFlight() && timer.elapsed() < cw::encodeShutdownChild::kStartTimeoutMs) {
            QThread::yieldCurrentThread();
        }
        return encodeIsInFlight();
    }
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    cwTask::initilizeThreadPool();

    const QImage image = gradientImage(cw::encodeShutdownChild::kImageSize);
    const QFuture<Monad::Result<QByteArray>> encode = cwConcurrent::run([image]() {
        return cw::ktx2::encodeRgba(image);
    });

    if(!waitForEncodeInFlight()) {
        return kExitEncodeNeverStarted;
    }

    std::fputs(cw::encodeShutdownChild::kInFlightMarker, stdout);
    std::fflush(stdout);

    //Returning here, with the encode still running, is the point of this process
    return 0;
}
