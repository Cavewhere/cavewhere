/**************************************************************************
**
**    Copyright (C) 2007-2026 Cavewhere contributors
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwFileUtils.h"

// Qt includes
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QThread>
#include <QTimer>

namespace {

bool waitForExistence(const QString& filePath, int timeoutMs) {
    QFileInfo file(filePath);
    if (file.exists()) {
        return true;
    }

    QString dirPath = QFileInfo(filePath).absolutePath();
    if (dirPath.isEmpty()) {
        return false;
    }

    QFileSystemWatcher watcher;
    if (!watcher.addPath(dirPath)) {
        return false;
    }

    QEventLoop loop;
    bool found = false;

    QObject::connect(&watcher, &QFileSystemWatcher::directoryChanged,
                     &loop, [&](const QString&) {
                         if (QFileInfo::exists(filePath)) {
                             found = true;
                             loop.quit();
                         }
                     });

    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);

    loop.exec();

    return found || QFileInfo::exists(filePath);
}

bool waitForSizeStability(const QString& filePath, int stabilityMs, int timeoutMs) {
    QElapsedTimer total;
    total.start();

    qint64 lastSize = -1;
    QElapsedTimer stableTimer;
    bool stableRunning = false;

    while (total.elapsed() < timeoutMs) {
        QFileInfo info(filePath);
        if (!info.exists()) {
            QThread::msleep(25);
            continue;
        }

        qint64 currentSize = info.size();
        if (currentSize == lastSize) {
            if (!stableRunning) {
                stableTimer.start();
                stableRunning = true;
            } else if (stableTimer.elapsed() >= stabilityMs) {
                return true;
            }
        } else {
            lastSize = currentSize;
            stableRunning = false;
        }

        QThread::msleep(25);
    }

    return false;
}

}


bool cwFileUtils::waitForFileReady(const QString& filePath,
                                   int existenceTimeoutMs,
                                   int stabilityMs,
                                   int stabilityTimeoutMs) {
    if (!waitForExistence(filePath, existenceTimeoutMs)) {
        return false;
    }

    return waitForSizeStability(filePath, stabilityMs, stabilityTimeoutMs);
}


