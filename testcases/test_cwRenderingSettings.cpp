//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwRenderingSettings.h"
#include "cwSignalSpy.h"
#include "SpyChecker.h"

//Qt includes
#include <QSettings>

TEST_CASE("cwRenderingSettings is a singleton defaulting to 4x MSAA", "[cwRenderingSettings]")
{
    cwRenderingSettings::initialize();
    auto settings = cwRenderingSettings::instance();
    cwRenderingSettings::initialize();
    auto settings2 = cwRenderingSettings::instance();
    CHECK(settings == settings2);
    REQUIRE(settings);

    // Restore the baseline supported set in case an earlier test narrowed it
    // (the singleton persists across test cases in a process).
    settings->setSupportedSampleCounts({1, 2, 4, 8});

    CHECK(settings->sampleCount() == 4);
    CHECK(settings->isAtDefaults());
    CHECK(settings->supportedSampleCounts() == QList<int>{1, 2, 4, 8});
}

TEST_CASE("cwRenderingSettings setter updates, clamps, persists, and emits", "[cwRenderingSettings]")
{
    cwRenderingSettings::initialize();
    auto settings = cwRenderingSettings::instance();
    REQUIRE(settings);
    settings->setSupportedSampleCounts({1, 2, 4, 8});
    settings->setSampleCount(4);

    cwSignalSpy sampleCountSpy(settings, &cwRenderingSettings::sampleCountChanged);
    sampleCountSpy.setObjectName("sampleCountSpy");

    SpyChecker checker = {
        {&sampleCountSpy, 0},
    };

    QSettings diskSettings;

    SECTION("a supported value updates, persists, and fires once") {
        settings->setSampleCount(8);
        checker[&sampleCountSpy]++;
        checker.checkSpies();

        CHECK(settings->sampleCount() == 8);
        CHECK(diskSettings.value(QStringLiteral("rendering/sampleCount")).toInt() == 8);
    }

    SECTION("out-of-set values snap down to the nearest supported count") {
        settings->setSampleCount(3);
        CHECK(settings->sampleCount() == 2);

        settings->setSampleCount(7);
        CHECK(settings->sampleCount() == 4);

        settings->setSampleCount(16);
        CHECK(settings->sampleCount() == 8);

        settings->setSampleCount(0);
        CHECK(settings->sampleCount() == 1);

        settings->setSampleCount(-5);
        CHECK(settings->sampleCount() == 1);
    }

    SECTION("setting the current value is a no-op and emits nothing") {
        settings->setSampleCount(settings->sampleCount());
        checker.checkSpies();
    }

    SECTION("snapping to the already-current value emits nothing") {
        settings->setSampleCount(8);
        checker[&sampleCountSpy]++;
        checker.checkSpies();

        // 7 snaps to 4 (changes from 8), then 5 snaps to 4 again (no-op).
        settings->setSampleCount(7);
        checker[&sampleCountSpy]++;
        checker.checkSpies();
        CHECK(settings->sampleCount() == 4);

        settings->setSampleCount(5);
        checker.checkSpies();
        CHECK(settings->sampleCount() == 4);
    }

    SECTION("resetToDefaults restores 4x") {
        settings->setSampleCount(1);
        CHECK(settings->sampleCount() == 1);
        CHECK_FALSE(settings->isAtDefaults());

        settings->resetToDefaults();
        CHECK(settings->sampleCount() == 4);
        CHECK(settings->isAtDefaults());
    }

    settings->setSupportedSampleCounts({1, 2, 4, 8});
    settings->setSampleCount(4);
}

TEST_CASE("cwRenderingSettings showRenderStatsHud round-trips, persists, and emits", "[cwRenderingSettings]")
{
    cwRenderingSettings::initialize();
    auto settings = cwRenderingSettings::instance();
    REQUIRE(settings);
    settings->setSupportedSampleCounts({1, 2, 4, 8});
    settings->setSampleCount(4);
    settings->setShowRenderStatsHud(false);

    cwSignalSpy hudSpy(settings, &cwRenderingSettings::showRenderStatsHudChanged);
    hudSpy.setObjectName("showRenderStatsHudSpy");

    SpyChecker checker = {
        {&hudSpy, 0},
    };

    QSettings diskSettings;

    SECTION("the HUD is off by default") {
        CHECK_FALSE(settings->showRenderStatsHud());
        CHECK(settings->isAtDefaults());
    }

    SECTION("enabling round-trips, persists to QSettings, and fires once") {
        settings->setShowRenderStatsHud(true);
        checker[&hudSpy]++;
        checker.checkSpies();

        CHECK(settings->showRenderStatsHud());
        CHECK(diskSettings.value(QStringLiteral("rendering/showRenderStatsHud")).toBool());
    }

    SECTION("setting the current value is a no-op and emits nothing") {
        settings->setShowRenderStatsHud(false);
        checker.checkSpies();
    }

    SECTION("the HUD participates in isAtDefaults and resetToDefaults") {
        settings->setShowRenderStatsHud(true);
        CHECK_FALSE(settings->isAtDefaults());

        settings->resetToDefaults();
        CHECK_FALSE(settings->showRenderStatsHud());
        CHECK(settings->sampleCount() == 4);
        CHECK(settings->isAtDefaults());
    }

    settings->setShowRenderStatsHud(false);
    settings->setSampleCount(4);
}

TEST_CASE("cwRenderingSettings budget knobs round-trip, clamp, persist, and emit", "[cwRenderingSettings]")
{
    cwRenderingSettings::initialize();
    auto settings = cwRenderingSettings::instance();
    REQUIRE(settings);
    settings->resetToDefaults();

    cwSignalSpy gpuSpy(settings, &cwRenderingSettings::gpuMemoryBudgetMbChanged);
    gpuSpy.setObjectName("gpuMemoryBudgetMbSpy");
    cwSignalSpy cpuSpy(settings, &cwRenderingSettings::cpuCacheBudgetMbChanged);
    cpuSpy.setObjectName("cpuCacheBudgetMbSpy");
    cwSignalSpy uploadSpy(settings, &cwRenderingSettings::uploadBudgetMbPerFrameChanged);
    uploadSpy.setObjectName("uploadBudgetMbPerFrameSpy");
    cwSignalSpy errorSpy(settings, &cwRenderingSettings::screenSpaceErrorPxChanged);
    errorSpy.setObjectName("screenSpaceErrorPxSpy");

    SpyChecker checker = {
        {&gpuSpy, 0},
        {&cpuSpy, 0},
        {&uploadSpy, 0},
        {&errorSpy, 0},
    };

    QSettings diskSettings;

    SECTION("the budgets start at their defaults") {
        CHECK(settings->gpuMemoryBudgetMb() == 1536);
        CHECK(settings->cpuCacheBudgetMb() == 512);
        CHECK(settings->uploadBudgetMbPerFrame() == 8);
        CHECK(settings->screenSpaceErrorPx() == Catch::Approx(1.5));
        CHECK(settings->isAtDefaults());
    }

    SECTION("each setter round-trips, persists to QSettings, and fires once") {
        settings->setGpuMemoryBudgetMb(2048);
        checker[&gpuSpy]++;
        settings->setCpuCacheBudgetMb(1024);
        checker[&cpuSpy]++;
        settings->setUploadBudgetMbPerFrame(16);
        checker[&uploadSpy]++;
        settings->setScreenSpaceErrorPx(2.5);
        checker[&errorSpy]++;
        checker.checkSpies();

        CHECK(settings->gpuMemoryBudgetMb() == 2048);
        CHECK(settings->cpuCacheBudgetMb() == 1024);
        CHECK(settings->uploadBudgetMbPerFrame() == 16);
        CHECK(settings->screenSpaceErrorPx() == Catch::Approx(2.5));

        CHECK(diskSettings.value(QStringLiteral("rendering/gpuMemoryBudgetMb")).toInt() == 2048);
        CHECK(diskSettings.value(QStringLiteral("rendering/cpuCacheBudgetMb")).toInt() == 1024);
        CHECK(diskSettings.value(QStringLiteral("rendering/uploadBudgetMbPerFrame")).toInt() == 16);
        CHECK(diskSettings.value(QStringLiteral("rendering/screenSpaceErrorPx")).toDouble() == Catch::Approx(2.5));

        CHECK_FALSE(settings->isAtDefaults());
    }

    SECTION("values below the minimum clamp up") {
        settings->setGpuMemoryBudgetMb(1);
        CHECK(settings->gpuMemoryBudgetMb() == 256);

        settings->setCpuCacheBudgetMb(-100);
        CHECK(settings->cpuCacheBudgetMb() == 64);

        settings->setUploadBudgetMbPerFrame(0);
        CHECK(settings->uploadBudgetMbPerFrame() == 1);

        settings->setScreenSpaceErrorPx(0.0);
        CHECK(settings->screenSpaceErrorPx() == Catch::Approx(0.5));
    }

    SECTION("values above the maximum clamp down") {
        settings->setGpuMemoryBudgetMb(1000000);
        CHECK(settings->gpuMemoryBudgetMb() == 65536);

        settings->setCpuCacheBudgetMb(1000000);
        CHECK(settings->cpuCacheBudgetMb() == 16384);

        settings->setUploadBudgetMbPerFrame(1000);
        CHECK(settings->uploadBudgetMbPerFrame() == 256);

        settings->setScreenSpaceErrorPx(100.0);
        CHECK(settings->screenSpaceErrorPx() == Catch::Approx(8.0));
    }

    SECTION("the exposed limits match the values the setters clamp to") {
        CHECK(cwRenderingSettings::minimumGpuMemoryBudgetMb() == 256);
        CHECK(cwRenderingSettings::maximumGpuMemoryBudgetMb() == 65536);
        CHECK(cwRenderingSettings::minimumCpuCacheBudgetMb() == 64);
        CHECK(cwRenderingSettings::maximumCpuCacheBudgetMb() == 16384);
        CHECK(cwRenderingSettings::minimumUploadBudgetMbPerFrame() == 1);
        CHECK(cwRenderingSettings::maximumUploadBudgetMbPerFrame() == 256);
        CHECK(cwRenderingSettings::minimumScreenSpaceErrorPx() == Catch::Approx(0.5));
        CHECK(cwRenderingSettings::maximumScreenSpaceErrorPx() == Catch::Approx(8.0));
    }

    SECTION("budgets() converts the megabyte knobs to bytes") {
        settings->setGpuMemoryBudgetMb(2048);
        settings->setCpuCacheBudgetMb(1024);
        settings->setUploadBudgetMbPerFrame(16);
        settings->setScreenSpaceErrorPx(2.5);

        CHECK(settings->gpuBudgetBytes() == qint64(2048) * cw::budgets::kBytesPerMegabyte);

        const cwRenderBudgets budgets = settings->budgets();
        CHECK(budgets.gpuBudgetBytes == qint64(2048) * cw::budgets::kBytesPerMegabyte);
        CHECK(budgets.cpuBudgetBytes == qint64(1024) * cw::budgets::kBytesPerMegabyte);
        CHECK(budgets.uploadBudgetBytesPerFrame == qint64(16) * cw::budgets::kBytesPerMegabyte);
        CHECK(budgets.screenSpaceErrorPx == Catch::Approx(2.5));
    }

    SECTION("the default budgets match a default-constructed cwRenderBudgets") {
        settings->resetToDefaults();

        const cwRenderBudgets defaults;
        const cwRenderBudgets budgets = settings->budgets();
        CHECK(budgets.gpuBudgetBytes == defaults.gpuBudgetBytes);
        CHECK(budgets.cpuBudgetBytes == defaults.cpuBudgetBytes);
        CHECK(budgets.uploadBudgetBytesPerFrame == defaults.uploadBudgetBytesPerFrame);
        CHECK(budgets.screenSpaceErrorPx == Catch::Approx(defaults.screenSpaceErrorPx));
    }

    SECTION("setting the current value is a no-op and emits nothing") {
        settings->setGpuMemoryBudgetMb(settings->gpuMemoryBudgetMb());
        settings->setCpuCacheBudgetMb(settings->cpuCacheBudgetMb());
        settings->setUploadBudgetMbPerFrame(settings->uploadBudgetMbPerFrame());
        settings->setScreenSpaceErrorPx(settings->screenSpaceErrorPx());
        checker.checkSpies();
    }

    SECTION("resetToDefaults restores all four and notifies") {
        settings->setGpuMemoryBudgetMb(4096);
        settings->setCpuCacheBudgetMb(2048);
        settings->setUploadBudgetMbPerFrame(32);
        settings->setScreenSpaceErrorPx(4.0);
        CHECK_FALSE(settings->isAtDefaults());

        gpuSpy.clear();
        cpuSpy.clear();
        uploadSpy.clear();
        errorSpy.clear();

        settings->resetToDefaults();
        checker[&gpuSpy]++;
        checker[&cpuSpy]++;
        checker[&uploadSpy]++;
        checker[&errorSpy]++;
        checker.checkSpies();

        CHECK(settings->gpuMemoryBudgetMb() == 1536);
        CHECK(settings->cpuCacheBudgetMb() == 512);
        CHECK(settings->uploadBudgetMbPerFrame() == 8);
        CHECK(settings->screenSpaceErrorPx() == Catch::Approx(1.5));
        CHECK(settings->isAtDefaults());
    }

    settings->resetToDefaults();
}

TEST_CASE("cwRenderingSettings honors the backend's supported sample counts", "[cwRenderingSettings]")
{
    cwRenderingSettings::initialize();
    auto settings = cwRenderingSettings::instance();
    REQUIRE(settings);
    settings->setSupportedSampleCounts({1, 2, 4, 8});
    settings->setSampleCount(4);

    SECTION("the list is sanitized: deduped, sorted ascending, and always offers 1") {
        settings->setSupportedSampleCounts({4, 2, 4, 1});
        CHECK(settings->supportedSampleCounts() == QList<int>{1, 2, 4});

        // 1 (no MSAA) must always be selectable even if the backend omits it.
        settings->setSupportedSampleCounts({8});
        CHECK(settings->supportedSampleCounts() == QList<int>{1, 8});

        // Junk (<= 0) is dropped.
        settings->setSupportedSampleCounts({1, 0, -4, 2});
        CHECK(settings->supportedSampleCounts() == QList<int>{1, 2});
    }

    SECTION("narrowing the supported set re-clamps the current selection") {
        settings->setSampleCount(8);
        CHECK(settings->sampleCount() == 8);

        cwSignalSpy sampleCountSpy(settings, &cwRenderingSettings::sampleCountChanged);
        sampleCountSpy.setObjectName("sampleCountSpy");

        // Metal-like set without 8: the current 8 snaps down to 4.
        settings->setSupportedSampleCounts({1, 2, 4});
        CHECK(settings->sampleCount() == 4);
        CHECK(sampleCountSpy.count() == 1);
    }

    SECTION("a still-supported selection survives a supported-set change") {
        settings->setSampleCount(2);
        CHECK(settings->sampleCount() == 2);

        cwSignalSpy sampleCountSpy(settings, &cwRenderingSettings::sampleCountChanged);
        settings->setSupportedSampleCounts({1, 2, 4});
        CHECK(settings->sampleCount() == 2);
        CHECK(sampleCountSpy.count() == 0);
    }

    settings->setSupportedSampleCounts({1, 2, 4, 8});
    settings->setSampleCount(4);
}
