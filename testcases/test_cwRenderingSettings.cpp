//Catch includes
#include <catch2/catch_test_macros.hpp>

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
