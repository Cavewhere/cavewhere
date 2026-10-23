#ifndef CWMEMORYBUDGET_H
#define CWMEMORYBUDGET_H

//Qt includes
#include <QObject>
#include <QThreadPool>

//Async includes
#include "asyncfuture.h"

//Our includes
#include "cwGlobals.h"
#include "cwTask.h"
#include "cwTaskTrace.h"

//Std includes
#include <atomic>

/**
 * The process-wide memory ledger that AsyncFuture::limited() charges on
 * admission (AsyncFuture::Limit::ledger). The cap is the memoryBudgetMb setting
 * when it is above zero, otherwise the platform's availableBytes() minus a
 * reserve for graphics and Qt, re-read on every charge. Thread-safe.
 */
class CAVEWHERE_LIB_EXPORT cwMemoryBudget : public QObject, public AsyncFuture::Ledger
{
    Q_OBJECT

public:
    explicit cwMemoryBudget(QObject* parent = nullptr);

    qint64 capBytes() const;
    qint64 automaticCapBytes() const;
    qint64 inFlightBytes() const;

    // 0 = automatic: the cap follows the platform reader.
    void setCapOverrideBytes(qint64 bytes);
    qint64 capOverrideBytes() const;

    // The pressure hook's seam: a paused ledger refuses every charge.
    void setPaused(bool paused);
    bool isPaused() const;

    bool tryCharge(qint64 bytes) override;
    void release(qint64 bytes) override;

    static qint64 availableBytes();
    static qint64 totalBytes();

    static cwMemoryBudget* instance();
    static void initialize();

    //! AsyncFuture::limited() as wide as the worker pool, with shareBytes as the pipeline's
    //! share of this ledger. Each launch's first pool task carries its weight in the task trace,
    //! and admissions, which run later from context's callbacks, carry the caller's trace context.
    template <typename T, typename Input, typename Launch, typename Weight>
    static AsyncFuture::Limited<T> limitedOnPool(QObject* context,
                                                 const QList<Input>& inputs,
                                                 Launch launch,
                                                 Weight weight,
                                                 qint64 shareBytes)
    {
        auto chargedLaunch = [launch = std::move(launch), weight](const Input& input) {
            const cwTaskChargeScope charge(weight(input));
            return launch(input);
        };

        const AsyncFuture::Limit limit {
            cwTask::threadPool()->maxThreadCount(),
            shareBytes,
            instance()
        };

        return AsyncFuture::limited<T>(context,
                                       inputs,
                                       cwTaskContextScope::wrap(std::move(chargedLaunch)),
                                       std::move(weight),
                                       limit);
    }

private:
    static cwMemoryBudget* Budget;

    std::atomic<qint64> m_inFlightBytes = 0;
    std::atomic<qint64> m_capOverrideBytes = 0;
    std::atomic<bool> m_paused = false;
};

#endif // CWMEMORYBUDGET_H
