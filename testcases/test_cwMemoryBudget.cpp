//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwMemoryBudget.h"
#include "cwJobSettings.h"
#include "cwRenderBudgets.h"

//Qt includes
#include <QSettings>
#include <QSignalSpy>
#include <QTest>

namespace {
    using cw::budgets::kBytesPerMegabyte;
    constexpr qint64 kCapBytes = 64 * kBytesPerMegabyte;
    constexpr qint64 kSmallerCapBytes = 32 * kBytesPerMegabyte;
    constexpr qint64 kLargeChargeBytes = 48 * kBytesPerMegabyte;
    constexpr qint64 kMediumChargeBytes = 16 * kBytesPerMegabyte;
    constexpr qint64 kOneMiB = kBytesPerMegabyte;
    constexpr int kOverrideMb = 7;
    constexpr int kTwoRunning = 2;
    constexpr int kWaitTimeoutMs = 1000;

    //! Restores the memoryBudgetMb setting, in QSettings and in cwJobSettings, on every path out of a test
    struct MemoryBudgetSettingRestorer {
        MemoryBudgetSettingRestorer() :
            m_hadValue(QSettings().contains(key())),
            m_storedValue(QSettings().value(key())),
            m_previous(cwJobSettings::instance()->memoryBudgetMb())
        {
        }

        ~MemoryBudgetSettingRestorer()
        {
            cwJobSettings::instance()->setMemoryBudgetMb(m_previous);
            QSettings settings;
            if(m_hadValue) {
                settings.setValue(key(), m_storedValue);
            } else {
                settings.remove(key());
            }
        }

        MemoryBudgetSettingRestorer(const MemoryBudgetSettingRestorer&) = delete;
        MemoryBudgetSettingRestorer& operator=(const MemoryBudgetSettingRestorer&) = delete;

    private:
        static QString key() { return QStringLiteral("memoryBudgetMb"); }

        bool m_hadValue;
        QVariant m_storedValue;
        int m_previous;
    };
}

TEST_CASE("cwMemoryBudget charges and releases bytes", "[cwMemoryBudget]") {
    cwMemoryBudget budget;
    budget.setCapOverrideBytes(kCapBytes);
    CHECK(budget.capBytes() == kCapBytes);
    CHECK(budget.inFlightBytes() == 0);

    REQUIRE(budget.tryCharge(kMediumChargeBytes));
    CHECK(budget.inFlightBytes() == kMediumChargeBytes);

    REQUIRE(budget.tryCharge(kMediumChargeBytes));
    CHECK(budget.inFlightBytes() == 2 * kMediumChargeBytes);

    budget.release(kMediumChargeBytes);
    CHECK(budget.inFlightBytes() == kMediumChargeBytes);

    budget.release(kMediumChargeBytes);
    CHECK(budget.inFlightBytes() == 0);
}

TEST_CASE("cwMemoryBudget refuses a charge past the cap", "[cwMemoryBudget]") {
    cwMemoryBudget budget;
    budget.setCapOverrideBytes(kCapBytes);

    REQUIRE(budget.tryCharge(kLargeChargeBytes));
    CHECK_FALSE(budget.tryCharge(kMediumChargeBytes + kOneMiB));
    CHECK(budget.inFlightBytes() == kLargeChargeBytes);

    // Exactly filling the cap fits.
    CHECK(budget.tryCharge(kCapBytes - kLargeChargeBytes));
    CHECK(budget.inFlightBytes() == kCapBytes);
    CHECK_FALSE(budget.tryCharge(1));
}

TEST_CASE("cwMemoryBudget with a smaller cap after a charge refuses the next charge", "[cwMemoryBudget]") {
    cwMemoryBudget budget;
    budget.setCapOverrideBytes(kCapBytes);
    REQUIRE(budget.tryCharge(kLargeChargeBytes));

    budget.setCapOverrideBytes(kSmallerCapBytes);
    CHECK(budget.capBytes() == kSmallerCapBytes);
    CHECK_FALSE(budget.tryCharge(kOneMiB));
    CHECK(budget.inFlightBytes() == kLargeChargeBytes);

    budget.release(kLargeChargeBytes);
    CHECK(budget.inFlightBytes() == 0);
    CHECK(budget.tryCharge(kMediumChargeBytes));
    CHECK(budget.inFlightBytes() == kMediumChargeBytes);
}

TEST_CASE("cwMemoryBudget memoryBudgetMb setting overrides the platform reader", "[cwMemoryBudget]") {
    cwJobSettings::initialize();
    auto* jobSettings = cwJobSettings::instance();
    auto* budget = cwMemoryBudget::instance();
    REQUIRE(budget != nullptr);

    MemoryBudgetSettingRestorer restorer;
    jobSettings->setMemoryBudgetMb(0);
    const bool wasAtDefaults = jobSettings->isAtDefaults();

    QSignalSpy changedSpy(jobSettings, &cwJobSettings::memoryBudgetMbChanged);

    jobSettings->setMemoryBudgetMb(kOverrideMb);
    CHECK(changedSpy.count() == 1);
    CHECK(jobSettings->memoryBudgetMb() == kOverrideMb);
    CHECK(QSettings().value(QStringLiteral("memoryBudgetMb")).toInt() == kOverrideMb);
    CHECK(budget->capOverrideBytes() == kOverrideMb * kBytesPerMegabyte);
    CHECK(budget->capBytes() == kOverrideMb * kBytesPerMegabyte);
    CHECK(jobSettings->resolvedMemoryBudgetMb() == kOverrideMb);
    CHECK_FALSE(jobSettings->isAtDefaults());

    jobSettings->setMemoryBudgetMb(0);
    CHECK(changedSpy.count() == 2);
    CHECK(jobSettings->memoryBudgetMb() == 0);
    CHECK(budget->capOverrideBytes() == 0);
    CHECK(budget->capBytes() > 0);
    CHECK(budget->capBytes() != kOverrideMb * kBytesPerMegabyte);
    CHECK(budget->capBytes() <= cwMemoryBudget::totalBytes());
    CHECK(jobSettings->resolvedMemoryBudgetMb() > 0);
    CHECK(jobSettings->isAtDefaults() == wasAtDefaults);
}

TEST_CASE("cwMemoryBudget platform reader reports positive totals", "[cwMemoryBudget]") {
    const qint64 total = cwMemoryBudget::totalBytes();
    const qint64 available = cwMemoryBudget::availableBytes();
    CHECK(total > 0);
    CHECK(available > 0);
    CHECK(available <= total);

    cwMemoryBudget budget;
    CHECK(budget.automaticCapBytes() > 0);
    CHECK(budget.automaticCapBytes() <= total);
}

TEST_CASE("cwMemoryBudget paused ledger refuses charges", "[cwMemoryBudget]") {
    cwMemoryBudget budget;
    budget.setCapOverrideBytes(kCapBytes);

    budget.setPaused(true);
    CHECK(budget.isPaused());
    CHECK_FALSE(budget.tryCharge(kOneMiB));
    CHECK(budget.inFlightBytes() == 0);

    // A zero-byte charge fits a paused ledger, matching AsyncFuture::ByteLedger.
    CHECK(budget.tryCharge(0));
    CHECK(budget.inFlightBytes() == 0);

    budget.setPaused(false);
    CHECK_FALSE(budget.isPaused());
    CHECK(budget.tryCharge(kOneMiB));
    CHECK(budget.inFlightBytes() == kOneMiB);
}

TEST_CASE("cwMemoryBudget unpausing wakes a waiting limited() run", "[cwMemoryBudget]") {
    cwMemoryBudget budget;
    budget.setCapOverrideBytes(kCapBytes);
    budget.setPaused(true);

    QObject context;
    QList<int> launchedInputs;
    QList<AsyncFuture::Deferred<int>> launched;
    auto launch = [&](int input) {
        AsyncFuture::Deferred<int> deferred;
        launchedInputs.append(input);
        launched.append(deferred);
        return deferred.future();
    };
    auto weight = [](int) { return kOneMiB; };

    auto run = AsyncFuture::limited(&context, QList<int>{0, 1}, launch, weight,
                                    AsyncFuture::Limit{kTwoRunning, 0, &budget});

    REQUIRE(launchedInputs == QList<int>{0});
    CHECK(budget.inFlightBytes() == kOneMiB);

    budget.setPaused(false);
    CHECK(QTest::qWaitFor([&]() { return launchedInputs.size() == kTwoRunning; }, kWaitTimeoutMs));
    CHECK(launchedInputs == QList<int>({0, 1}));
    CHECK_FALSE(run.items.at(0).isFinished());

    const auto completeLaunchedUntilAllFinish = [&]() {
        for (int i = 0; i < launched.size(); ++i) {
            AsyncFuture::Deferred<int> deferred = launched.at(i);
            deferred.complete(launchedInputs.at(i));
        }
        return run.all.isFinished();
    };
    CHECK(QTest::qWaitFor(completeLaunchedUntilAllFinish, kWaitTimeoutMs));
    CHECK(budget.inFlightBytes() == 0);
}

TEST_CASE("cwMemoryBudget release of zero bytes emits no released()", "[cwMemoryBudget]") {
    cwMemoryBudget budget;
    budget.setCapOverrideBytes(kCapBytes);
    REQUIRE(budget.tryCharge(kOneMiB));

    QSignalSpy releasedSpy(&budget, &cwMemoryBudget::released);
    budget.release(0);
    CHECK(releasedSpy.count() == 0);
    CHECK(budget.inFlightBytes() == kOneMiB);

    budget.release(kOneMiB);
    CHECK(releasedSpy.count() == 1);
    CHECK(budget.inFlightBytes() == 0);
}
