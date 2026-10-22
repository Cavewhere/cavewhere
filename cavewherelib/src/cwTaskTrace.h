#ifndef CWTASKTRACE_H
#define CWTASKTRACE_H

//Qt includes
#include <QFuture>
#include <QList>
#include <QString>

//Std includes
#include <chrono>

//Our includes
#include "CaveWhereLibExport.h"
#include <MarkScope/Profiler.h>

class cwRootData;
class QObject;
struct cwTaskContextCounters;

constexpr int kTaskTraceSampleIntervalMs = 50;
constexpr int kTaskTraceQuietWindowMs = 2000;

//! The context a thread tags its submissions with
struct cwTaskContext
{
    QString name;
    quint64 generation = 0;
    cwTaskContextCounters* counters = nullptr;
};

//! RAII thread-local context tag. Scopes nest: the destructor restores the previous context.
class CAVEWHERE_LIB_EXPORT cwTaskContextScope
{
public:
    //! A generation of 0 means the work belongs to no pipeline generation
    explicit cwTaskContextScope(const QString& context, quint64 generation = 0);
    explicit cwTaskContextScope(const cwTaskContext& context);
    ~cwTaskContextScope();
    cwTaskContextScope(const cwTaskContextScope&) = delete;
    cwTaskContextScope& operator=(const cwTaskContextScope&) = delete;

    //! The current thread's context, or "untagged" when no scope is active
    static QString current();

    //! The current thread's full context, for reinstalling in a continuation that runs on another thread
    static cwTaskContext currentContext();

private:
    cwTaskContext m_previous;
};

//! A task interval for work that bypasses cwConcurrent::run, such as cwTask and direct pool->start() calls
class CAVEWHERE_LIB_EXPORT cwTaskInterval
{
public:
    //! waitUs is how long the task sat in the queue, when known
    explicit cwTaskInterval(const QString& name, qint64 waitUs = 0);
    ~cwTaskInterval();
    cwTaskInterval(const cwTaskInterval&) = delete;
    cwTaskInterval& operator=(const cwTaskInterval&) = delete;

private:
    MarkScope::Profiler::Token m_token;
    bool m_traced;
};

//! One task submitted through cwConcurrent::run. Counts as queued from construction until it runs or is destroyed.
class CAVEWHERE_LIB_EXPORT cwTaskSubmission
{
public:
    cwTaskSubmission();
    cwTaskSubmission(cwTaskSubmission&& other) noexcept;
    ~cwTaskSubmission();
    cwTaskSubmission(const cwTaskSubmission&) = delete;
    cwTaskSubmission& operator=(const cwTaskSubmission&) = delete;
    cwTaskSubmission& operator=(cwTaskSubmission&&) = delete;

    //! Lives on the worker for the length of the task: reinstalls the context, counts it as running, and marks it
    class CAVEWHERE_LIB_EXPORT Run
    {
    public:
        explicit Run(cwTaskSubmission& submission);
        ~Run();
        Run(const Run&) = delete;
        Run& operator=(const Run&) = delete;

    private:
        cwTaskContextScope m_scope;
        cwTaskContextCounters* m_counters;
        cwTaskInterval m_interval;
    };

private:
    cwTaskContext m_context;
    std::chrono::steady_clock::time_point m_submitted;
    bool m_pending;
};

//! A batch interval for cwConcurrent::map/mapped, open from submission until the kernel releases it
class CAVEWHERE_LIB_EXPORT cwTaskBatch
{
public:
    cwTaskBatch(const QString& name, qsizetype count);
    ~cwTaskBatch();

    //! Ends the interval ahead of destruction
    void finish();
    cwTaskBatch(const cwTaskBatch&) = delete;
    cwTaskBatch& operator=(const cwTaskBatch&) = delete;

private:
    MarkScope::Profiler::Token m_token;
    bool m_traced;
};

class CAVEWHERE_LIB_EXPORT cwTaskTrace
{
public:
    struct ContextCounts
    {
        QString context;
        int queued;
        int running;
    };

    cwTaskTrace() = delete;

    //! Per-context queued/running counts kept by cwConcurrent::run. The sampler reads the same call.
    static QList<ContextCounts> contextCounts();

    //! Marks a pipeline's results for a generation as dropped by the generation check
    static void dropped(const QString& context, quint64 generation);

    //! A coarse batch interval in the current context that ends when future finishes
    static void traceOperation(const QString& name, const QFuture<void>& future, const QObject* context);

    //! Starts the sampler thread and the idle watcher; quits the app on idle when quitOnIdle is set
    static void start(cwRootData* rootData, bool quitOnIdle);
};

//! Snapshot of the idle conditions
struct CAVEWHERE_LIB_EXPORT cwTaskQuietState
{
    bool projectLoaded = false;
    bool coordinatorRunning = false;
    bool automaticUpdate = false;
    bool needsUpdate = false;
    int futureCount = 0;
    int activeThreadCount = 0;
    int queuedTaskCount = 0;
    int runningTaskCount = 0;

    bool isQuiet() const;
};

//! Tracks how long the state has been continuously quiet
class CAVEWHERE_LIB_EXPORT cwTaskIdleWindow
{
public:
    //! True once the state has stayed quiet for kTaskTraceQuietWindowMs
    bool update(const cwTaskQuietState& state, qint64 nowMs);

private:
    qint64 m_quietSinceMs = -1;
};

#endif // CWTASKTRACE_H
