//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwRenderingSettings.h"
#include "cwSignalSpy.h"
#include "SpyChecker.h"

//Qt includes
#include <QSettings>
#include <QSignalSpy>
#include <QVector>

//Std includes
#include <functional>

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
    cwSignalSpy pointSpy(settings, &cwRenderingSettings::pointBudgetMillionsChanged);
    pointSpy.setObjectName("pointBudgetMillionsSpy");

    SpyChecker checker = {
        {&gpuSpy, 0},
        {&cpuSpy, 0},
        {&uploadSpy, 0},
        {&errorSpy, 0},
        {&pointSpy, 0},
    };

    QSettings diskSettings;

    SECTION("the budgets start at their defaults") {
        CHECK(settings->gpuMemoryBudgetMb() == 1536);
        CHECK(settings->cpuCacheBudgetMb() == 512);
        CHECK(settings->uploadBudgetMbPerFrame() == 8);
        CHECK(settings->screenSpaceErrorPx() == Catch::Approx(1.5));
        CHECK(settings->pointBudgetMillions() == 16);
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
        settings->setPointBudgetMillions(32);
        checker[&pointSpy]++;
        checker.checkSpies();

        CHECK(settings->gpuMemoryBudgetMb() == 2048);
        CHECK(settings->cpuCacheBudgetMb() == 1024);
        CHECK(settings->uploadBudgetMbPerFrame() == 16);
        CHECK(settings->screenSpaceErrorPx() == Catch::Approx(2.5));
        CHECK(settings->pointBudgetMillions() == 32);

        CHECK(diskSettings.value(QStringLiteral("rendering/gpuMemoryBudgetMb")).toInt() == 2048);
        CHECK(diskSettings.value(QStringLiteral("rendering/cpuCacheBudgetMb")).toInt() == 1024);
        CHECK(diskSettings.value(QStringLiteral("rendering/uploadBudgetMbPerFrame")).toInt() == 16);
        CHECK(diskSettings.value(QStringLiteral("rendering/screenSpaceErrorPx")).toDouble() == Catch::Approx(2.5));
        CHECK(diskSettings.value(QStringLiteral("rendering/pointBudgetMillions")).toInt() == 32);

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

        settings->setPointBudgetMillions(0);
        CHECK(settings->pointBudgetMillions() == 1);
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

        settings->setPointBudgetMillions(100000);
        CHECK(settings->pointBudgetMillions() == 512);
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
        CHECK(cwRenderingSettings::minimumPointBudgetMillions() == 1);
        CHECK(cwRenderingSettings::maximumPointBudgetMillions() == 512);
    }

    SECTION("budgets() converts the megabyte knobs to bytes") {
        settings->setGpuMemoryBudgetMb(2048);
        settings->setCpuCacheBudgetMb(1024);
        settings->setUploadBudgetMbPerFrame(16);
        settings->setScreenSpaceErrorPx(2.5);
        settings->setPointBudgetMillions(32);

        CHECK(settings->gpuBudgetBytes() == qint64(2048) * cw::budgets::kBytesPerMegabyte);

        const cwRenderBudgets budgets = settings->budgets();
        CHECK(budgets.gpuBudgetBytes == qint64(2048) * cw::budgets::kBytesPerMegabyte);
        CHECK(budgets.cpuBudgetBytes == qint64(1024) * cw::budgets::kBytesPerMegabyte);
        CHECK(budgets.uploadBudgetBytesPerFrame == qint64(16) * cw::budgets::kBytesPerMegabyte);
        CHECK(budgets.screenSpaceErrorPx == Catch::Approx(2.5));
        CHECK(budgets.pointBudget == qint64(32) * cw::budgets::kPointsPerMillion);
    }

    SECTION("the default budgets match a default-constructed cwRenderBudgets") {
        settings->resetToDefaults();

        const cwRenderBudgets defaults;
        const cwRenderBudgets budgets = settings->budgets();
        CHECK(budgets.gpuBudgetBytes == defaults.gpuBudgetBytes);
        CHECK(budgets.cpuBudgetBytes == defaults.cpuBudgetBytes);
        CHECK(budgets.uploadBudgetBytesPerFrame == defaults.uploadBudgetBytesPerFrame);
        CHECK(budgets.screenSpaceErrorPx == Catch::Approx(defaults.screenSpaceErrorPx));
        CHECK(budgets.pointBudget == defaults.pointBudget);
    }

    SECTION("setting the current value is a no-op and emits nothing") {
        settings->setGpuMemoryBudgetMb(settings->gpuMemoryBudgetMb());
        settings->setCpuCacheBudgetMb(settings->cpuCacheBudgetMb());
        settings->setUploadBudgetMbPerFrame(settings->uploadBudgetMbPerFrame());
        settings->setScreenSpaceErrorPx(settings->screenSpaceErrorPx());
        settings->setPointBudgetMillions(settings->pointBudgetMillions());
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
        CHECK(settings->pointBudgetMillions() == 16);
        CHECK(settings->isAtDefaults());
    }

    settings->resetToDefaults();
}

TEST_CASE("cwRenderingSettings point cloud knobs round-trip, persist, and flip isAtDefaults",
          "[cwRenderingSettings]")
{
    cwRenderingSettings::initialize();
    auto settings = cwRenderingSettings::instance();
    REQUIRE(settings);
    settings->resetToDefaults();

    using cw::pointcloud::DebugView;
    using cw::pointcloud::FloorMode;
    using cw::pointcloud::SizeMode;
    using cw::pointcloud::SpacingSource;
    using cw::pointcloud::ThinningMode;

    constexpr int kCappedLevel = 3;
    constexpr int kIsolatedLevel = 2;
    constexpr double kFixedPointSizePx = 5.0;
    constexpr double kMaxPointSizePx = 12.0;
    constexpr double kGovernorSlew = 0.25;

    // One knob each: how to move it off its default, what QSettings holds
    // afterward, and the notify signal that has to fire once.
    struct Knob {
        const char* name;
        std::function<void()> change;
        QString key;
        QVariant persisted;
        void (cwRenderingSettings::*notify)();
    };

    const QVector<Knob> knobs = {
        {"freezeCut", [&] { settings->setFreezeCut(true); },
         QStringLiteral("rendering/pointCloud/freezeCut"), true,
         &cwRenderingSettings::freezeCutChanged},
        {"maxLevel", [&] { settings->setMaxLevel(kCappedLevel); },
         QStringLiteral("rendering/pointCloud/maxLevel"), kCappedLevel,
         &cwRenderingSettings::maxLevelChanged},
        {"minLevel", [&] { settings->setMinLevel(kIsolatedLevel); },
         QStringLiteral("rendering/pointCloud/minLevel"), kIsolatedLevel,
         &cwRenderingSettings::minLevelChanged},
        {"inflationGovernor", [&] { settings->setInflationGovernor(false); },
         QStringLiteral("rendering/pointCloud/inflationGovernor"), false,
         &cwRenderingSettings::inflationGovernorChanged},
        {"governorSlew", [&] { settings->setGovernorSlew(kGovernorSlew); },
         QStringLiteral("rendering/pointCloud/governorSlew"), kGovernorSlew,
         &cwRenderingSettings::governorSlewChanged},
        {"evictionHold", [&] { settings->setEvictionHold(true); },
         QStringLiteral("rendering/pointCloud/evictionHold"), true,
         &cwRenderingSettings::evictionHoldChanged},
        {"thinningMode", [&] { settings->setThinningMode(int(ThinningMode::Discrete)); },
         QStringLiteral("rendering/pointCloud/thinningMode"), int(ThinningMode::Discrete),
         &cwRenderingSettings::thinningModeChanged},
        {"sizeMode", [&] { settings->setSizeMode(int(SizeMode::FixedPixels)); },
         QStringLiteral("rendering/pointCloud/sizeMode"), int(SizeMode::FixedPixels),
         &cwRenderingSettings::sizeModeChanged},
        {"floorMode", [&] { settings->setFloorMode(int(FloorMode::Off)); },
         QStringLiteral("rendering/pointCloud/floorMode"), int(FloorMode::Off),
         &cwRenderingSettings::floorModeChanged},
        {"spacingSource", [&] { settings->setSpacingSource(int(SpacingSource::Measured)); },
         QStringLiteral("rendering/pointCloud/spacingSource"), int(SpacingSource::Measured),
         &cwRenderingSettings::spacingSourceChanged},
        {"fixedPointSizePx", [&] { settings->setFixedPointSizePx(kFixedPointSizePx); },
         QStringLiteral("rendering/pointCloud/fixedPointSizePx"), kFixedPointSizePx,
         &cwRenderingSettings::fixedPointSizePxChanged},
        {"maxPointSizePx", [&] { settings->setMaxPointSizePx(kMaxPointSizePx); },
         QStringLiteral("rendering/pointCloud/maxPointSizePx"), kMaxPointSizePx,
         &cwRenderingSettings::maxPointSizePxChanged},
        {"debugView", [&] { settings->setDebugView(int(DebugView::ResidencyAge)); },
         QStringLiteral("rendering/pointCloud/debugView"), int(DebugView::ResidencyAge),
         &cwRenderingSettings::debugViewChanged},
        {"showNodeBoxes", [&] { settings->setShowNodeBoxes(true); },
         QStringLiteral("rendering/pointCloud/showNodeBoxes"), true,
         &cwRenderingSettings::showNodeBoxesChanged},
    };

    SECTION("the knobs start at the renderer's own rules") {
        const cwRenderBudgets defaults;
        const cwRenderBudgets budgets = settings->budgets();
        CHECK(budgets.freezeCut == defaults.freezeCut);
        CHECK(budgets.maxLevel == defaults.maxLevel);
        CHECK(budgets.maxLevel == cwRenderingSettings::unlimitedLevel());
        CHECK(budgets.minLevel == defaults.minLevel);
        CHECK(budgets.minLevel == cwRenderingSettings::rootLevel());
        CHECK(budgets.inflationGovernor == defaults.inflationGovernor);
        CHECK(budgets.evictionHold == defaults.evictionHold);
        CHECK(settings->pointCloudShading() == cwPointCloudShading());
        CHECK(settings->maxPointSizePx() == Catch::Approx(cw::pointcloud::kMaxPointSizePx));
        CHECK(settings->debugView() == int(DebugView::Off));
        CHECK_FALSE(settings->showNodeBoxes());
        CHECK(settings->isAtDefaults());
    }

    for (const Knob& knob : knobs) {
        DYNAMIC_SECTION("moving " << knob.name << " persists it, fires once, and leaves the defaults") {
            QSignalSpy notifySpy(settings, knob.notify);
            QSignalSpy defaultsSpy(settings, &cwRenderingSettings::isAtDefaultsChanged);

            knob.change();

            CHECK(notifySpy.count() == 1);
            CHECK(defaultsSpy.count() == 1);
            CHECK_FALSE(settings->isAtDefaults());

            QSettings diskSettings;
            CHECK(diskSettings.value(knob.key) == knob.persisted);

            //Setting the same value again is a no-op
            knob.change();
            CHECK(notifySpy.count() == 1);

            settings->resetPointCloudKnobsToDefaults();
            CHECK(notifySpy.count() == 2);
            CHECK(settings->isAtDefaults());
        }
    }

    SECTION("the streaming knobs reach cwRenderBudgets") {
        settings->setFreezeCut(true);
        settings->setMaxLevel(kCappedLevel);
        settings->setMinLevel(kIsolatedLevel);
        settings->setInflationGovernor(false);
        settings->setEvictionHold(true);

        const cwRenderBudgets budgets = settings->budgets();
        CHECK(budgets.freezeCut);
        CHECK(budgets.maxLevel == kCappedLevel);
        CHECK(budgets.minLevel == kIsolatedLevel);
        CHECK_FALSE(budgets.inflationGovernor);
        CHECK(budgets.evictionHold);
    }

    SECTION("the shader knobs reach pointCloudShading and announce it") {
        QSignalSpy shadingSpy(settings, &cwRenderingSettings::pointCloudShadingChanged);

        settings->setThinningMode(int(ThinningMode::Off));
        settings->setSizeMode(int(SizeMode::OwnSpacing));
        settings->setFloorMode(int(FloorMode::Off));
        settings->setFixedPointSizePx(kFixedPointSizePx);
        settings->setMaxPointSizePx(kMaxPointSizePx);
        settings->setDebugView(int(DebugView::DecimationPreview));
        settings->setShowNodeBoxes(true);
        CHECK(shadingSpy.count() == 7);

        const cwPointCloudShading& shading = settings->pointCloudShading();
        CHECK(shading.thinningMode == ThinningMode::Off);
        CHECK(shading.sizeMode == SizeMode::OwnSpacing);
        CHECK(shading.floorMode == FloorMode::Off);
        CHECK(shading.fixedPointSizePx == Catch::Approx(kFixedPointSizePx));
        CHECK(shading.maxPointSizePx == Catch::Approx(kMaxPointSizePx));
        CHECK(shading.debugView == DebugView::DecimationPreview);
        CHECK(shading.showNodeBoxes);
    }

    SECTION("minLevel and maxLevel stay ordered, the edited knob wins") {
        settings->setMaxLevel(kIsolatedLevel);
        settings->setMinLevel(kCappedLevel);
        CHECK(settings->minLevel() == kCappedLevel);
        CHECK(settings->maxLevel() == kCappedLevel);

        settings->setMaxLevel(kIsolatedLevel);
        CHECK(settings->maxLevel() == kIsolatedLevel);
        CHECK(settings->minLevel() == kIsolatedLevel);

        settings->setMaxLevel(cwRenderingSettings::unlimitedLevel());
        settings->setMinLevel(kCappedLevel);
        CHECK(settings->maxLevel() == cwRenderingSettings::unlimitedLevel());
        CHECK(settings->minLevel() == kCappedLevel);

        const cwRenderBudgets budgets = settings->budgets();
        CHECK(budgets.minLevel == kCappedLevel);
        CHECK(budgets.maxLevel == cwRenderingSettings::unlimitedLevel());
    }

    SECTION("out-of-range values clamp") {
        settings->setMaxLevel(-10);
        CHECK(settings->maxLevel() == cwRenderingSettings::unlimitedLevel());
        settings->setMaxLevel(1000);
        CHECK(settings->maxLevel() == cwRenderingSettings::maximumLevel());

        settings->setMinLevel(-1);
        CHECK(settings->minLevel() == cwRenderingSettings::rootLevel());
        settings->setMinLevel(1000);
        CHECK(settings->minLevel() == cwRenderingSettings::maximumLevel());

        settings->setDebugView(-1);
        CHECK(settings->debugView() == int(DebugView::Off));
        settings->setDebugView(100);
        CHECK(settings->debugView() == cwRenderingSettings::debugViewCount() - 1);

        settings->setThinningMode(-1);
        CHECK(settings->thinningMode() == 0);
        settings->setThinningMode(100);
        CHECK(settings->thinningMode() == cwRenderingSettings::thinningModeCount() - 1);
        settings->setSizeMode(100);
        CHECK(settings->sizeMode() == cwRenderingSettings::sizeModeCount() - 1);
        settings->setFloorMode(100);
        CHECK(settings->floorMode() == cwRenderingSettings::floorModeCount() - 1);

        settings->setFixedPointSizePx(0.0);
        CHECK(settings->fixedPointSizePx() == Catch::Approx(cwRenderingSettings::minimumPointSizePx()));
        settings->setMaxPointSizePx(1000.0);
        CHECK(settings->maxPointSizePx() == Catch::Approx(cwRenderingSettings::maximumPointSizePx()));
    }

    SECTION("resetting the point cloud knobs leaves MSAA and the HUD alone") {
        settings->setShowRenderStatsHud(true);
        settings->setThinningMode(int(ThinningMode::Off));

        settings->resetPointCloudKnobsToDefaults();
        CHECK(settings->thinningMode() == int(ThinningMode::Continuous));
        CHECK(settings->showRenderStatsHud());
    }

    settings->resetToDefaults();
}

TEST_CASE("cwRenderingSettings governorSlew defaults to the governor's own step, clamps, and persists",
          "[cwRenderingSettings]")
{
    cwRenderingSettings::initialize();
    auto settings = cwRenderingSettings::instance();
    REQUIRE(settings);
    settings->resetToDefaults();

    constexpr double kQuarterSlew = 0.25;
    constexpr double kBelowRange = 0.0;
    constexpr double kAboveRange = 2.0;
    const QString key = QStringLiteral("rendering/pointCloud/governorSlew");

    SECTION("the default jumps to the target in one frame") {
        CHECK(cw::budgets::kDefaultGovernorSlew == 1.0);
        CHECK(settings->governorSlew() == Catch::Approx(cw::budgets::kDefaultGovernorSlew));
        CHECK(cwRenderBudgets().governorSlew == Catch::Approx(cw::budgets::kDefaultGovernorSlew));
        CHECK(settings->budgets().governorSlew == Catch::Approx(cw::budgets::kDefaultGovernorSlew));
        CHECK(settings->isAtDefaults());
    }

    SECTION("the range is the open-closed unit interval the forest steps over") {
        CHECK(cwRenderingSettings::minimumGovernorSlew() == Catch::Approx(cw::budgets::kMinGovernorSlew));
        CHECK(cwRenderingSettings::maximumGovernorSlew() == Catch::Approx(cw::budgets::kMaxGovernorSlew));
        CHECK(cwRenderingSettings::minimumGovernorSlew() > 0.0);
        CHECK(cwRenderingSettings::maximumGovernorSlew() == Catch::Approx(1.0));
        CHECK(cwRenderingSettings::minimumGovernorSlew() < cwRenderingSettings::maximumGovernorSlew());
    }

    SECTION("values outside the range clamp to its ends") {
        settings->setGovernorSlew(kBelowRange);
        CHECK(settings->governorSlew() == Catch::Approx(cwRenderingSettings::minimumGovernorSlew()));
        settings->setGovernorSlew(kAboveRange);
        CHECK(settings->governorSlew() == Catch::Approx(cwRenderingSettings::maximumGovernorSlew()));
    }

    SECTION("a slew round-trips through QSettings and reaches the budgets") {
        QSignalSpy slewSpy(settings, &cwRenderingSettings::governorSlewChanged);
        QSignalSpy budgetsSpy(settings, &cwRenderingSettings::budgetsChanged);
        QSignalSpy defaultsSpy(settings, &cwRenderingSettings::isAtDefaultsChanged);

        settings->setGovernorSlew(kQuarterSlew);

        CHECK(slewSpy.count() == 1);
        CHECK(budgetsSpy.count() == 1);
        CHECK(defaultsSpy.count() == 1);
        CHECK(settings->governorSlew() == Catch::Approx(kQuarterSlew));
        CHECK(settings->budgets().governorSlew == Catch::Approx(kQuarterSlew));
        CHECK_FALSE(settings->isAtDefaults());

        QSettings diskSettings;
        CHECK(diskSettings.value(key).toDouble() == Catch::Approx(kQuarterSlew));

        settings->setGovernorSlew(kQuarterSlew);
        CHECK(slewSpy.count() == 1);
        CHECK(budgetsSpy.count() == 1);

        settings->resetPointCloudKnobsToDefaults();
        CHECK(settings->governorSlew() == Catch::Approx(cw::budgets::kDefaultGovernorSlew));
        CHECK(diskSettings.value(key).toDouble() == Catch::Approx(cw::budgets::kDefaultGovernorSlew));
        CHECK(settings->isAtDefaults());
    }

    settings->resetToDefaults();
}

TEST_CASE("cwRenderingSettings spacingSource defaults to the grid, clamps, and persists",
          "[cwRenderingSettings]")
{
    cwRenderingSettings::initialize();
    auto settings = cwRenderingSettings::instance();
    REQUIRE(settings);
    settings->resetToDefaults();

    using cw::pointcloud::SpacingSource;
    const QString key = QStringLiteral("rendering/pointCloud/spacingSource");

    SECTION("the default sizes and thins by the grid, as the renderer always did") {
        CHECK(cwPointCloudShading().spacingSource == SpacingSource::Grid);
        CHECK(settings->spacingSource() == int(SpacingSource::Grid));
        CHECK(settings->pointCloudShading().spacingSource == SpacingSource::Grid);
        CHECK(cwRenderingSettings::spacingSourceCount() == cw::pointcloud::kSpacingSourceCount);
        CHECK(cwRenderingSettings::spacingSourceCount() == 2);
        CHECK(settings->isAtDefaults());
    }

    SECTION("values outside the enum clamp to its ends") {
        settings->setSpacingSource(-1);
        CHECK(settings->spacingSource() == int(SpacingSource::Grid));
        settings->setSpacingSource(100);
        CHECK(settings->spacingSource() == cwRenderingSettings::spacingSourceCount() - 1);
        CHECK(settings->spacingSource() == int(SpacingSource::Measured));
    }

    SECTION("measured round-trips through QSettings and reaches the shading") {
        QSignalSpy sourceSpy(settings, &cwRenderingSettings::spacingSourceChanged);
        QSignalSpy shadingSpy(settings, &cwRenderingSettings::pointCloudShadingChanged);
        QSignalSpy budgetsSpy(settings, &cwRenderingSettings::budgetsChanged);
        QSignalSpy defaultsSpy(settings, &cwRenderingSettings::isAtDefaultsChanged);

        settings->setSpacingSource(int(SpacingSource::Measured));

        CHECK(sourceSpy.count() == 1);
        CHECK(shadingSpy.count() == 1);
        CHECK(budgetsSpy.count() == 0);
        CHECK(defaultsSpy.count() == 1);
        CHECK(settings->spacingSource() == int(SpacingSource::Measured));
        CHECK(settings->pointCloudShading().spacingSource == SpacingSource::Measured);
        CHECK_FALSE(settings->isAtDefaults());

        QSettings diskSettings;
        CHECK(diskSettings.value(key).toInt() == int(SpacingSource::Measured));

        settings->setSpacingSource(int(SpacingSource::Measured));
        CHECK(sourceSpy.count() == 1);

        settings->resetPointCloudKnobsToDefaults();
        CHECK(settings->spacingSource() == int(SpacingSource::Grid));
        CHECK(diskSettings.value(key).toInt() == int(SpacingSource::Grid));
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

TEST_CASE("cwRenderingSettings announces every budget knob through budgetsChanged",
          "[cwRenderingSettings]")
{
    cwRenderingSettings::initialize();
    auto settings = cwRenderingSettings::instance();
    REQUIRE(settings);
    settings->resetToDefaults();

    constexpr int kCappedLevel = 3;
    constexpr int kIsolatedLevel = 2;
    constexpr double kFixedPointSizePx = 5.0;
    constexpr double kMaxPointSizePx = 12.0;

    // budgetsChanged is what asks the scene for a frame when one of these
    // moves, so a knob missing from it would wait for the camera to move
    struct Knob {
        const char* name;
        std::function<void()> change;
        bool rideBudgets;
    };

    const QVector<Knob> knobs = {
        {"gpuMemoryBudgetMb", [&] { settings->setGpuMemoryBudgetMb(cw::budgets::kMinGpuBudgetMb); }, true},
        {"cpuCacheBudgetMb", [&] { settings->setCpuCacheBudgetMb(cw::budgets::kMinCpuBudgetMb); }, true},
        {"uploadBudgetMbPerFrame",
         [&] { settings->setUploadBudgetMbPerFrame(cw::budgets::kMinUploadBudgetMbPerFrame); }, true},
        {"screenSpaceErrorPx",
         [&] { settings->setScreenSpaceErrorPx(cw::budgets::kMaxScreenSpaceErrorPx); }, true},
        {"pointBudgetMillions",
         [&] { settings->setPointBudgetMillions(cw::budgets::kMinPointBudgetMillions); }, true},
        {"freezeCut", [&] { settings->setFreezeCut(true); }, true},
        {"maxLevel", [&] { settings->setMaxLevel(kCappedLevel); }, true},
        {"minLevel", [&] { settings->setMinLevel(kIsolatedLevel); }, true},
        {"inflationGovernor", [&] { settings->setInflationGovernor(false); }, true},
        {"governorSlew",
         [&] { settings->setGovernorSlew(cw::budgets::kMinGovernorSlew); }, true},
        {"evictionHold", [&] { settings->setEvictionHold(true); }, true},
        {"thinningMode",
         [&] { settings->setThinningMode(int(cw::pointcloud::ThinningMode::Discrete)); }, false},
        {"sizeMode", [&] { settings->setSizeMode(int(cw::pointcloud::SizeMode::FixedPixels)); }, false},
        {"floorMode", [&] { settings->setFloorMode(int(cw::pointcloud::FloorMode::Off)); }, false},
        {"spacingSource",
         [&] { settings->setSpacingSource(int(cw::pointcloud::SpacingSource::Measured)); }, false},
        {"fixedPointSizePx", [&] { settings->setFixedPointSizePx(kFixedPointSizePx); }, false},
        {"maxPointSizePx", [&] { settings->setMaxPointSizePx(kMaxPointSizePx); }, false},
        {"debugView",
         [&] { settings->setDebugView(int(cw::pointcloud::DebugView::ResidencyAge)); }, false},
        {"showNodeBoxes", [&] { settings->setShowNodeBoxes(true); }, false},
    };

    for (const Knob& knob : knobs) {
        INFO(knob.name);
        settings->resetToDefaults();

        QSignalSpy budgetsSpy(settings, &cwRenderingSettings::budgetsChanged);
        QSignalSpy shadingSpy(settings, &cwRenderingSettings::pointCloudShadingChanged);

        knob.change();

        // Each knob rides exactly one of the two ways to the render thread
        if (knob.rideBudgets) {
            CHECK(budgetsSpy.count() == 1);
            CHECK(shadingSpy.count() == 0);
        } else {
            CHECK(budgetsSpy.count() == 0);
            CHECK(shadingSpy.count() == 1);
        }

        // Setting the value it already holds announces nothing
        knob.change();
        CHECK(budgetsSpy.count() + shadingSpy.count() == 1);
    }

    settings->resetToDefaults();
}
