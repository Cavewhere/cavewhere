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

    template<typename T>
    class Restarter {
    public:
        Restarter(QObject* context) :
            Context(context)
        {

        }

        Restarter(const Restarter& other) = delete;
        Restarter& operator=(const Restarter& other) = delete;

        void restart(std::function<QFuture<T> ()> runFunction) {
            Q_ASSERT(runFunction);

            if(!Future.isRunning()) {
                setFuture(runFunction());
            } else {
                //Update the run function so we use the most update one
                this->runFunction = runFunction;

                //Only setup the watch and cancel the future once
                if(!isCanceled) {
                    //Watch for when the Future is cancelled
                    AsyncFuture::observe(Future).context(Context,
                                [](){}, //Do nothing on finished
                    [this]()
                    {
                        //Recursive call
                        Q_ASSERT(Future.isCanceled());
                        setFuture(this->runFunction());
                    });

                    //Cancel
                    isCanceled = true;
                    Future.cancel();
                }
            }

        }

        void onFutureChanged(std::function<void ()> changedCallback) {
            this->changedCallback = changedCallback;
        }

        QFuture<T> future() const {
            return Future;
        }

    private:
        std::function<QFuture<T> ()> runFunction;
        std::function<void ()> changedCallback;
        QFuture<T> Future;
        QObject* Context;
        bool isCanceled = false;

        void setFuture(QFuture<T> future) {
            isCanceled = false;
            Future = future;
            if(changedCallback) {
                changedCallback();
            }
        }
    };
};

#endif // CWASYNCFUTURE_H
