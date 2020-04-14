#ifndef CWASYNCFUTURE_H
#define CWASYNCFUTURE_H

//Qt includes
#include <QFuture>
#include <QEventLoop>
#include <QFutureWatcher>
#include <QTimer>

//AsyncFuture
#include "asyncfuture.h"

class cwAsyncFuture
{
public:
    cwAsyncFuture() = delete;

    template<class T>
    static bool waitForFinished(QFuture<T> future, int timeout = -1) {
        if (future.isFinished()) {
            return true;
        }

        QFutureWatcher<T> watcher;
        QEventLoop loop;

        if (timeout > 0) {
            QTimer::singleShot(timeout, &loop, &QEventLoop::quit);
        }

        QObject::connect(&watcher, SIGNAL(finished()), &loop, SLOT(quit()));

        watcher.setFuture(future);

        loop.exec();

        return watcher.isFinished();
    }

    template<class T, typename RunFunc>
    static void restart(QFuture<T>* future, RunFunc run) {
        future->cancel();

        if(!future->isRunning()) {
            *future = run();

            AsyncFuture::observe(*future).subscribe(
                        [](){},
            [future, run]()
            {
                //Recursive call
                *future = QFuture<T>();
                Q_ASSERT(!future->isRunning());
                restart(future, run);
            });
        }
    }
};

#endif // CWASYNCFUTURE_H
