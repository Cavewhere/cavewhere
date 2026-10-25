#ifndef VADOSECONCURRENT_H
#define VADOSECONCURRENT_H

//Qt includes
#include <QtConcurrent>
#include <QCoreApplication>
#include <QThreadPool>
#include "CaveWhereLibExport.h"
#include "cwTask.h"
#include "cwTaskTrace.h"

//Std includes
#include <functional>
#include <iterator>
#include <memory>
#include <type_traits>

class CAVEWHERE_LIB_EXPORT cwConcurrent
{
public:
    cwConcurrent() = delete;

    //! Tags the task with the submitting thread's cwTaskContextScope and marks it on the worker
    template <class Function, class ...Args>
    static auto run(Function &&f, Args &&...args)
    {
        using Callable = std::decay_t<Function>;
        if constexpr (std::is_invocable_v<Callable, std::decay_t<Args>...>) {
            return QtConcurrent::run(cwTask::threadPool(),
                                     [submission = cwTaskSubmission(), f = Callable(std::forward<Function>(f))]
                                     (std::decay_t<Args>... arguments) mutable {
                                         cwTaskSubmission::Run run(submission);
                                         return std::invoke(f, std::move(arguments)...);
                                     },
                                     std::forward<Args>(args)...);
        } else {
            using PromiseType = typename QtPrivate::ArgResolver<Callable>::PromiseType;
            return QtConcurrent::run(cwTask::threadPool(),
                                     [submission = cwTaskSubmission(), f = Callable(std::forward<Function>(f))]
                                     (QPromise<PromiseType>& promise, std::decay_t<Args>... arguments) mutable {
                                         cwTaskSubmission::Run run(submission);
                                         std::invoke(f, promise, std::move(arguments)...);
                                     },
                                     std::forward<Args>(args)...);
        }
    }

    template <typename Sequence, typename MapFunctor>
    static auto mapped(
        Sequence &&sequence,
        MapFunctor &&map)
    {
        return QtConcurrent::mapped(cwTask::threadPool(), sequence, batchFunctor(QStringLiteral("mapped"), sequence, map));
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
        return QtConcurrent::map(cwTask::threadPool(), sequence, batchFunctor(QStringLiteral("map"), sequence, map));
    }

private:
    //! The kernel holds the functor until the batch finishes, so its copy of the batch closes the interval then.
    //! Each item is its own task interval on the worker, so its CPU lands on the context like a run() would
    template <typename Sequence, typename MapFunctor>
    static auto batchFunctor(const QString& name, const Sequence& sequence, MapFunctor &&map)
    {
        auto batch = std::make_shared<const cwTaskBatch>(name, static_cast<qsizetype>(std::size(sequence)));
        return [batch, name, context = cwTaskContextScope::currentContext(), map = std::decay_t<MapFunctor>(std::forward<MapFunctor>(map))](auto&& item) mutable -> decltype(auto) {
            cwTaskContextScope scope(context);
            cwTaskInterval interval(name);
            return std::invoke(map, std::forward<decltype(item)>(item));
        };
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
