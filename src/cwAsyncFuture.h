#ifndef CWASYNCFUTURE_H
#define CWASYNCFUTURE_H

//Qt includes
#include <QFuture>
#include <QEventLoop>
#include <QTimer>

class cwAsyncFuture
{
public:
    cwAsyncFuture() = delete;

    template<class T>
    static void waitForFinished(QFuture<T> future, int timeout = -1) {
        if (future.isFinished()) {
            return;
        }

        QFutureWatcher<T> watcher;
        QEventLoop loop;

        if (timeout > 0) {
            QTimer::singleShot(timeout, &loop, &QEventLoop::quit);
        }

        QObject::connect(&watcher, SIGNAL(finished()), &loop, SLOT(quit()));

        watcher.setFuture(future);

        loop.exec();
    }
};

#endif // CWASYNCFUTURE_H
