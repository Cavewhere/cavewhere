#ifndef VADOSECONCURRENT_H
#define VADOSECONCURRENT_H

//Qt includes
#include <QtConcurrent>
#include <QCoreApplication>
#include <QThreadPool>
#include "CaveWhereLibExport.h"
#include "cwTask.h"

class CAVEWHERE_LIB_EXPORT cwConcurrent
{
public:
    cwConcurrent() = delete;

    template <class Function, class ...Args>
    static auto run(Function &&f, Args &&...args)
    {
        return QtConcurrent::run(cwTask::threadPool(), std::forward<Function>(f), std::forward<Args>(args)...);
    }

    template <typename Sequence, typename MapFunctor>
    static auto mapped(
        Sequence &&sequence,
        MapFunctor &&map)
    {
        return QtConcurrent::mapped(cwTask::threadPool(), sequence, map);
    }

    // Fire-and-forget parallel iteration: like mapped() but the functor
    // returns void and the future is QFuture<void>. Callers that only need
    // to side-effect (fill a shared buffer, push progress, etc.) should
    // prefer this over mapped() to avoid storing dummy result values.
    template <typename Sequence, typename MapFunctor>
    static auto map(
        Sequence &&sequence,
        MapFunctor &&map)
    {
        return QtConcurrent::map(cwTask::threadPool(), sequence, map);
    }


    // static void startThreadPool() {
    //     //I'm not sure if these ratios are the best
    //     // const int main = std::max(1, QThread::idealThreadCount() * 3 / 4);
    //     const int worker = std::max(1, QThread::idealThreadCount() * 3 / 4);
    //     const int main = std::max(1, QThread::idealThreadCount());
    //     // const int worker = std::max(1, QThread::idealThreadCount());
    //     // const int main = 1;
    //     // const int worker = 1;

    //     QThreadPool::globalInstance()->setMaxThreadCount(main);
    //     // QThreadPool::globalInstance()->setThreadPriority(QThread::HighPriority);
    //     if(!cwConcurrent::m_threadPool) {
    //         cwConcurrent::m_threadPool = new QThreadPool(QCoreApplication::instance());
    //         cwConcurrent::m_threadPool->setThreadPriority(QThread::LowestPriority);
    //         cwConcurrent::m_threadPool->setMaxThreadCount(worker);
    //     }
    // }

private:
    // static QThreadPool* m_threadPool;

};

#endif // VADOSECONCURRENT_H
