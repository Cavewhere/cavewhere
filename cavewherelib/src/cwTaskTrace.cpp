//Our includes
#include "cwTaskTrace.h"
#include "cwFutureManagerModel.h"
#include "cwKtx2Codec.h"
#include "cwNoteLiDARManager.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwScrapManager.h"
#include "cwTask.h"
#include "cwUpdateCoordinator.h"

//Async includes
#include "asyncfuture.h"

//Qt includes
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHash>
#include <QMutex>
#include <QThreadPool>
#include <QTimer>

//Std includes
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#ifdef Q_OS_APPLE
#include "cwTaskTraceApple.h"
#include <mach/mach.h>
#endif
#ifdef Q_OS_IOS
#include <os/proc.h>
#endif

struct cwTaskContextCounters
{
    explicit cwTaskContextCounters(const QString& name) :
        name(name)
    {
    }

    const QString name;
    std::atomic<int> queued{0};
    std::atomic<int> running{0};
};

namespace {
constexpr int kTaskTraceEnabledPollMs = 1000;

QString untaggedName()
{
    return QStringLiteral("untagged");
}

MarkScope::Profiler& profiler()
{
    //Its own category keeps task intervals out of the Points of Interest track
    static MarkScope::Profiler instance(QStringLiteral("com.cavewhere.tasks"), QStringLiteral("tasks"));
    return instance;
}

//! Counters live for the whole process so a cwTaskContext can hold a raw pointer to them
class CounterRegistry
{
public:
    cwTaskContextCounters* counters(const QString& name)
    {
        QMutexLocker locker(&m_mutex);
        for (const auto& counters : m_counters) {
            if (counters->name == name) {
                return counters.get();
            }
        }
        m_counters.push_back(std::make_unique<cwTaskContextCounters>(name));
        return m_counters.back().get();
    }

    QList<cwTaskTrace::ContextCounts> snapshot()
    {
        QMutexLocker locker(&m_mutex);
        QList<cwTaskTrace::ContextCounts> counts;
        counts.reserve(static_cast<qsizetype>(m_counters.size()));
        for (const auto& counters : m_counters) {
            counts.append({counters->name,
                           counters->queued.load(std::memory_order_relaxed),
                           counters->running.load(std::memory_order_relaxed)});
        }
        return counts;
    }

private:
    QMutex m_mutex;
    std::vector<std::unique_ptr<cwTaskContextCounters>> m_counters;
};

CounterRegistry& registry()
{
    static CounterRegistry instance;
    return instance;
}

cwTaskContext untaggedContext()
{
    static const cwTaskContext untagged {untaggedName(), 0, registry().counters(untaggedName())};
    return untagged;
}

cwTaskContext& threadContext()
{
    thread_local cwTaskContext context = untaggedContext();
    return context;
}

qint64& threadCharge()
{
    thread_local qint64 bytes = 0;
    return bytes;
}

struct PoolSample
{
    QString name;
    QThreadPool* pool;
};

QList<PoolSample> pools()
{
    return {
        {QStringLiteral("cwTask"), cwTask::threadPool()},
        {QStringLiteral("global"), QThreadPool::globalInstance()},
        {QStringLiteral("ktx2"), cw::ktx2::encodeLane()}
    };
}

void emitMemorySample()
{
    if (const std::optional<cwTaskMemorySample> sample = cwTaskTrace::memorySample()) {
        profiler().event(sample->message());
    }
}

void emitSample()
{
    emitMemorySample();

    const QList<cwTaskTrace::ContextCounts> counts = cwTaskTrace::contextCounts();
    for (const cwTaskTrace::ContextCounts& context : counts) {
        profiler().event(QStringLiteral("queue ctx=%1 queued=%2 running=%3")
                             .arg(context.context)
                             .arg(context.queued)
                             .arg(context.running));
    }

    const QList<PoolSample> samples = pools();
    for (const PoolSample& sample : samples) {
        profiler().event(QStringLiteral("pool name=%1 active=%2 max=%3")
                             .arg(sample.name)
                             .arg(sample.pool->activeThreadCount())
                             .arg(sample.pool->maxThreadCount()));
    }
}

//! Samples at kTaskTraceSampleIntervalMs while a recorder is attached, and checks for one once a second otherwise
class Sampler
{
public:
    Sampler() :
        m_thread([this]() { loop(); })
    {
    }

    ~Sampler()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopping = true;
        }
        m_wake.notify_all();
        m_thread.join();
    }

private:
    void loop()
    {
        MarkScope::Profiler::setThreadName(QStringLiteral("cwTaskTrace sampler"));
        std::unique_lock<std::mutex> lock(m_mutex);
        while (!m_stopping) {
            const bool recording = profiler().enabled();
            if (recording) {
                emitSample();
            }
            const int intervalMs = recording ? kTaskTraceSampleIntervalMs : kTaskTraceEnabledPollMs;
            m_wake.wait_for(lock, std::chrono::milliseconds(intervalMs), [this]() { return m_stopping; });
        }
    }

    std::mutex m_mutex;
    std::condition_variable m_wake;
    bool m_stopping = false;
    std::thread m_thread;
};

struct IdleWatch
{
    cwTaskIdleWindow window;
    QElapsedTimer clock;
    bool projectLoaded = false;
    bool idleReported = false;
    bool redirtyPending = false;
};

cwTaskQuietState quietState(const cwRootData* rootData, const IdleWatch& watch)
{
    const cwUpdateCoordinator* coordinator = rootData->updateCoordinator();

    cwTaskQuietState state;
    state.projectLoaded = watch.projectLoaded;
    state.redirtyPending = watch.redirtyPending;
    state.coordinatorRunning = coordinator->running();
    state.automaticUpdate = coordinator->automaticUpdate();
    state.needsUpdate = coordinator->needsUpdate();
    state.futureCount = rootData->futureManagerModel()->count();

    const QList<PoolSample> samples = pools();
    for (const PoolSample& sample : samples) {
        state.activeThreadCount += sample.pool->activeThreadCount();
    }

    const QList<cwTaskTrace::ContextCounts> counts = cwTaskTrace::contextCounts();
    for (const cwTaskTrace::ContextCounts& context : counts) {
        state.queuedTaskCount += context.queued;
        state.runningTaskCount += context.running;
    }
    return state;
}

//Marks every scrap and LiDAR note dirty once, delay after the project loads, so
//a recording catches an edit landing on a busy run
void armRedirty(cwRootData* rootData, const std::shared_ptr<IdleWatch>& watch, std::chrono::milliseconds delay)
{
    watch->redirtyPending = true;

    QObject::connect(rootData->project(), &cwProject::loaded, rootData, [rootData, watch, delay]() {
        QTimer::singleShot(delay, rootData, [rootData, watch]() {
            profiler().event(QStringLiteral("redirty elapsed_ms=%1").arg(watch->clock.elapsed()));
            rootData->scrapManager()->markAllScrapsDirty();
            rootData->noteLiDARManager()->markAllLiDARNotesDirty();
            rootData->updateCoordinator()->updateNow();
            watch->redirtyPending = false;
        });
    }, Qt::SingleShotConnection);
}

void startIdleWatcher(cwRootData* rootData, bool quitOnIdle, double redirtyAfterSeconds)
{
    auto watch = std::make_shared<IdleWatch>();
    watch->clock.start();

    QObject::connect(rootData->project(), &cwProject::loaded, rootData, [watch]() {
        watch->projectLoaded = true;
    });

    if (redirtyAfterSeconds > 0.0) {
        const std::chrono::duration<double> seconds(redirtyAfterSeconds);
        armRedirty(rootData, watch, std::chrono::duration_cast<std::chrono::milliseconds>(seconds));
    }

    auto* timer = new QTimer(rootData);
    QObject::connect(timer, &QTimer::timeout, rootData, [rootData, timer, watch, quitOnIdle]() {
        const bool watching = quitOnIdle || profiler().enabled();
        if (watching) {
            timer->setInterval(kTaskTraceSampleIntervalMs);
        } else {
            timer->setInterval(kTaskTraceEnabledPollMs);
            watch->window = cwTaskIdleWindow();
            watch->idleReported = false;
            return;
        }

        const bool idle = watch->window.update(quietState(rootData, *watch), watch->clock.elapsed());
        if (idle && !watch->idleReported) {
            watch->idleReported = true;
            profiler().event(QStringLiteral("idle elapsed_ms=%1").arg(watch->clock.elapsed()));
            if (quitOnIdle) {
                QCoreApplication::quit();
            }
        }
        watch->idleReported = idle;
    });
    timer->start(kTaskTraceSampleIntervalMs);
}
}

cwTaskContextScope::cwTaskContextScope(const QString& context, quint64 generation) :
    cwTaskContextScope(cwTaskContext {context, generation, registry().counters(context)})
{
}

cwTaskContextScope::cwTaskContextScope(const cwTaskContext& context) :
    m_previous(threadContext())
{
    threadContext() = context;
}

cwTaskContextScope::~cwTaskContextScope()
{
    threadContext() = m_previous;
}

QString cwTaskContextScope::current()
{
    return threadContext().name;
}

cwTaskContext cwTaskContextScope::currentContext()
{
    return threadContext();
}

cwTaskChargeScope::cwTaskChargeScope(qint64 bytes) :
    m_previous(std::exchange(threadCharge(), bytes))
{
}

cwTaskChargeScope::~cwTaskChargeScope()
{
    threadCharge() = m_previous;
}

cwTaskSubmission::cwTaskSubmission() :
    m_context(threadContext()),
    m_submitted(std::chrono::steady_clock::now()),
    m_chargedBytes(std::exchange(threadCharge(), 0)),
    m_pending(true)
{
    m_context.counters->queued.fetch_add(1, std::memory_order_relaxed);
}

cwTaskSubmission::cwTaskSubmission(cwTaskSubmission&& other) noexcept :
    m_context(std::move(other.m_context)),
    m_submitted(other.m_submitted),
    m_chargedBytes(other.m_chargedBytes),
    m_pending(other.m_pending)
{
    other.m_pending = false;
}

cwTaskSubmission::~cwTaskSubmission()
{
    //A task canceled before it started never ran, so it leaves the queue here
    if (m_pending) {
        m_context.counters->queued.fetch_sub(1, std::memory_order_relaxed);
    }
}

cwTaskSubmission::Run::Run(cwTaskSubmission& submission) :
    m_scope(submission.m_context),
    m_counters(submission.m_context.counters),
    m_interval(QStringLiteral("run"),
               std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now() - submission.m_submitted).count(),
               submission.m_chargedBytes)
{
    Q_ASSERT(submission.m_pending);
    submission.m_pending = false;
    m_counters->queued.fetch_sub(1, std::memory_order_relaxed);
    m_counters->running.fetch_add(1, std::memory_order_relaxed);
}

cwTaskSubmission::Run::~Run()
{
    m_counters->running.fetch_sub(1, std::memory_order_relaxed);
}

cwTaskBatch::cwTaskBatch(const QString& name, qsizetype count) :
    m_traced(profiler().enabled())
{
    if (m_traced) {
        const cwTaskContext& context = threadContext();
        m_token = profiler().start(MarkScope::Profiler::Kind::Batch,
                                   QStringLiteral("ctx=%1 name=%2 n=%3 gen=%4")
                                       .arg(context.name, name)
                                       .arg(count)
                                       .arg(context.generation));
    }
}

cwTaskBatch::~cwTaskBatch()
{
    finish();
}

void cwTaskBatch::finish()
{
    if (m_traced) {
        m_traced = false;
        profiler().stop(m_token);
    }
}

cwTaskInterval::cwTaskInterval(const QString& name, qint64 waitUs, qint64 chargedBytes) :
    m_traced(profiler().enabled())
{
    if (m_traced) {
        m_token = profiler().start(metadata(threadContext(), name, waitUs, chargedBytes));
    }
}

QString cwTaskInterval::metadata(const cwTaskContext& context, const QString& name, qint64 waitUs, qint64 chargedBytes)
{
    QString metadata = QStringLiteral("ctx=%1 name=%2 wait_us=%3 gen=%4")
                           .arg(context.name, name)
                           .arg(waitUs)
                           .arg(context.generation);
    if (chargedBytes > 0) {
        metadata += QStringLiteral(" charged_bytes=%1").arg(chargedBytes);
    }
    return metadata;
}

cwTaskInterval::~cwTaskInterval()
{
    if (m_traced) {
        profiler().stop(m_token);
    }
}

QString cwTaskMemorySample::message() const
{
    QString message = QStringLiteral("mem footprint=%1 resident=%2").arg(footprint).arg(resident);
    if (gpu) {
        message += QStringLiteral(" gpu=%1").arg(*gpu);
    }
    if (available) {
        message += QStringLiteral(" available=%1").arg(*available);
    }
    return message;
}

std::optional<cwTaskMemorySample> cwTaskTrace::memorySample()
{
#ifdef Q_OS_APPLE
    task_vm_info_data_t info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS) {
        return std::nullopt;
    }

    cwTaskMemorySample sample;
    sample.footprint = info.phys_footprint;
    sample.resident = info.resident_size;
    sample.gpu = cwMetalAllocatedBytes();
#ifdef Q_OS_IOS
    sample.available = os_proc_available_memory();
#endif
    return sample;
#else
    return std::nullopt;
#endif
}

QList<cwTaskTrace::ContextCounts> cwTaskTrace::contextCounts()
{
    return registry().snapshot();
}

void cwTaskTrace::dropped(const QString& context, quint64 generation)
{
    //Generations only grow, so remembering the last one per context reports each dropped generation once
    static QMutex mutex;
    static QHash<QString, quint64> lastReported;
    {
        QMutexLocker locker(&mutex);
        const auto reported = lastReported.constFind(context);
        if (reported != lastReported.constEnd() && reported.value() == generation) {
            return;
        }
        lastReported.insert(context, generation);
    }

    if (profiler().enabled()) {
        profiler().event(QStringLiteral("dropped ctx=%1 gen=%2").arg(context).arg(generation));
    }
}

void cwTaskTrace::traceOperation(const QString& name, const QFuture<void>& future, const QObject* context)
{
    auto batch = std::make_shared<cwTaskBatch>(name, 1);
    const auto finish = [batch]() { batch->finish(); };
    AsyncFuture::observe(future).context(context, finish, finish);
}

void cwTaskTrace::start(cwRootData* rootData, bool quitOnIdle, double redirtyAfterSeconds)
{
    static bool started = false;
    if (started) {
        return;
    }
    started = true;

    //Built before the sampler so static destruction tears the sampler down first; its thread reads the profiler until join()
    profiler();
    static std::unique_ptr<Sampler> sampler = std::make_unique<Sampler>();

    //The sampler reads the pools, which the application tears down after aboutToQuit
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, qApp, []() {
        sampler.reset();
    });

    startIdleWatcher(rootData, quitOnIdle, redirtyAfterSeconds);
}

bool cwTaskQuietState::isQuiet() const
{
    const bool updatePending = automaticUpdate && needsUpdate;
    return projectLoaded
           && !coordinatorRunning
           && !updatePending
           && futureCount == 0
           && activeThreadCount == 0
           && queuedTaskCount == 0
           && runningTaskCount == 0
           && !redirtyPending;
}

bool cwTaskIdleWindow::update(const cwTaskQuietState& state, qint64 nowMs)
{
    if (state.isQuiet()) {
        if (m_quietSinceMs < 0) {
            m_quietSinceMs = nowMs;
        }
        return nowMs - m_quietSinceMs >= kTaskTraceQuietWindowMs;
    }

    m_quietSinceMs = -1;
    return false;
}

MarkScope::Profiler& cwTaskProfiler()
{
    return profiler();
}
