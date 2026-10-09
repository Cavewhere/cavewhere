//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwPassageTubeSettings.h"
#include "cwSignalSpy.h"
#include "SpyChecker.h"

//Qt includes
#include <QSettings>

namespace {
constexpr const char* kSampleSpacingMetersKey = "passageTube/sampleSpacingMeters";
constexpr const char* kRingResolutionKey = "passageTube/ringResolution";
}

TEST_CASE("cwPassageTubeSettings is a singleton that starts at its defaults", "[PassageTube][cwPassageTubeSettings]")
{
    cwPassageTubeSettings::initialize();
    auto settings = cwPassageTubeSettings::instance();
    cwPassageTubeSettings::initialize();
    CHECK(settings == cwPassageTubeSettings::instance());
    REQUIRE(settings);

    settings->resetToDefaults();

    CHECK(settings->sampleSpacingMeters() == Catch::Approx(1.0));
    CHECK(settings->ringResolution() == 16);
    CHECK(settings->isAtDefaults());
}

TEST_CASE("cwPassageTubeSettings setters round-trip, clamp, persist, and emit", "[PassageTube][cwPassageTubeSettings]")
{
    cwPassageTubeSettings::initialize();
    auto settings = cwPassageTubeSettings::instance();
    REQUIRE(settings);
    settings->resetToDefaults();

    cwSignalSpy spacingSpy(settings, &cwPassageTubeSettings::sampleSpacingMetersChanged);
    spacingSpy.setObjectName("sampleSpacingMetersSpy");
    cwSignalSpy resolutionSpy(settings, &cwPassageTubeSettings::ringResolutionChanged);
    resolutionSpy.setObjectName("ringResolutionSpy");
    cwSignalSpy defaultsSpy(settings, &cwPassageTubeSettings::isAtDefaultsChanged);
    defaultsSpy.setObjectName("isAtDefaultsSpy");

    SpyChecker checker = {
        {&spacingSpy, 0},
        {&resolutionSpy, 0},
        {&defaultsSpy, 0},
    };

    QSettings diskSettings;

    SECTION("each setter updates, persists to QSettings, and fires once") {
        settings->setSampleSpacingMeters(2.5);
        checker[&spacingSpy]++;
        checker[&defaultsSpy]++;
        settings->setRingResolution(24);
        checker[&resolutionSpy]++;
        checker[&defaultsSpy]++;
        checker.checkSpies();

        CHECK(settings->sampleSpacingMeters() == Catch::Approx(2.5));
        CHECK(settings->ringResolution() == 24);
        CHECK(diskSettings.value(kSampleSpacingMetersKey).toDouble() == Catch::Approx(2.5));
        CHECK(diskSettings.value(kRingResolutionKey).toInt() == 24);
        CHECK_FALSE(settings->isAtDefaults());
    }

    SECTION("values outside the range clamp to its edges") {
        settings->setSampleSpacingMeters(0.0);
        CHECK(settings->sampleSpacingMeters() == Catch::Approx(0.1));

        settings->setSampleSpacingMeters(1000.0);
        CHECK(settings->sampleSpacingMeters() == Catch::Approx(50.0));

        settings->setRingResolution(1);
        CHECK(settings->ringResolution() == 4);

        settings->setRingResolution(500);
        CHECK(settings->ringResolution() == 64);

        CHECK(diskSettings.value(kSampleSpacingMetersKey).toDouble() == Catch::Approx(50.0));
        CHECK(diskSettings.value(kRingResolutionKey).toInt() == 64);
    }

    SECTION("setting the current value emits nothing") {
        settings->setSampleSpacingMeters(settings->sampleSpacingMeters());
        settings->setRingResolution(settings->ringResolution());
        checker.checkSpies();
    }

    SECTION("a value that clamps to the current one emits nothing") {
        settings->setRingResolution(64);
        checker[&resolutionSpy]++;
        checker[&defaultsSpy]++;
        checker.checkSpies();

        settings->setRingResolution(100);
        checker.checkSpies();
    }

    SECTION("resetToDefaults restores both and notifies") {
        settings->setSampleSpacingMeters(5.0);
        settings->setRingResolution(8);
        CHECK_FALSE(settings->isAtDefaults());

        spacingSpy.clear();
        resolutionSpy.clear();
        defaultsSpy.clear();

        settings->resetToDefaults();
        checker[&spacingSpy]++;
        checker[&resolutionSpy]++;
        checker[&defaultsSpy] += 2;
        checker.checkSpies();

        CHECK(settings->sampleSpacingMeters() == Catch::Approx(1.0));
        CHECK(settings->ringResolution() == 16);
        CHECK(settings->isAtDefaults());
    }

    settings->resetToDefaults();
}

TEST_CASE("cwPassageTubeSettings persists across instances", "[PassageTube][cwPassageTubeSettings]")
{
    cwPassageTubeSettings first;
    first.setSampleSpacingMeters(3.5);
    first.setRingResolution(32);

    cwPassageTubeSettings second;
    CHECK(second.sampleSpacingMeters() == Catch::Approx(3.5));
    CHECK(second.ringResolution() == 32);
    CHECK_FALSE(second.isAtDefaults());

    second.resetToDefaults();
    cwPassageTubeSettings third;
    CHECK(third.sampleSpacingMeters() == Catch::Approx(1.0));
    CHECK(third.ringResolution() == 16);
    CHECK(third.isAtDefaults());
}

TEST_CASE("cwPassageTubeSettings clamps out-of-range values read from QSettings", "[PassageTube][cwPassageTubeSettings]")
{
    {
        QSettings diskSettings;
        diskSettings.setValue(kSampleSpacingMetersKey, -4.0);
        diskSettings.setValue(kRingResolutionKey, 1000);
    }

    cwPassageTubeSettings settings;
    CHECK(settings.sampleSpacingMeters() == Catch::Approx(0.1));
    CHECK(settings.ringResolution() == 64);

    settings.resetToDefaults();
}
