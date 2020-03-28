//Our includes
#include "TaskWithBlockingAsync.h"
#include "cwAsyncFuture.h"

//Qt includes
#include <QThread>
#include <QtConcurrent>

//AsyncFuture includes
#include "asyncfuture.h"

TaskWithBlockingAsync::TaskWithBlockingAsync()
{

}

void TaskWithBlockingAsync::runTask()
{
    auto context = QSharedPointer<QObject>::create();

    QVector<int> ints(100);
    std::iota(ints.begin(), ints.end(), ints.size());
    std::function<int (int)> func = [](int x)->int {
        QThread::msleep(10);
        return x * x;
    };
    QFuture<int> mappedFuture = QtConcurrent::mapped(ints, func);

    auto sumFuture = AsyncFuture::observe(mappedFuture)
            .context(context.get(),
                     [mappedFuture]()
    {
        auto results = mappedFuture.results();
        int sum = std::accumulate(results.begin(), results.end(), 0);
        return sum;
    }).future();

    cwAsyncFuture::waitForFinished(sumFuture);

    Result = sumFuture.result();

    done();
}
