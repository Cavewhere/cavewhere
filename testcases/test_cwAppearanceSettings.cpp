//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwAppearanceSettings.h"
#include "cwSignalSpy.h"
#include "SpyChecker.h"

//Qt includes
#include <QSettings>

TEST_CASE("cwAppearanceSettings should follow and persist the color scheme", "[cwAppearanceSettings]") {
    QSettings diskSettings;

    cwAppearanceSettings::initialize();
    auto settings = cwAppearanceSettings::instance();

    cwAppearanceSettings::initialize();
    CHECK(cwAppearanceSettings::instance() == settings);
    REQUIRE(settings);

    // The singleton survives across sections, so every section starts from System
    settings->setColorScheme(cwAppearanceSettings::System);

    cwSignalSpy colorSchemeSpy(settings, &cwAppearanceSettings::colorSchemeChanged);
    cwSignalSpy darkSpy(settings, &cwAppearanceSettings::darkChanged);

    colorSchemeSpy.setObjectName("colorSchemeSpy");
    darkSpy.setObjectName("darkSpy");

    SpyChecker checker = {
        {&colorSchemeSpy, 0},
        {&darkSpy, 0}
    };

    SECTION("Default is System") {
        CHECK(settings->colorScheme() == cwAppearanceSettings::System);
    }

    SECTION("Setting Dark makes dark() true and signals once each") {
        // Start from Light so darkChanged fires on a host whose system scheme is dark
        settings->setColorScheme(cwAppearanceSettings::Light);
        colorSchemeSpy.clear();
        darkSpy.clear();

        settings->setColorScheme(cwAppearanceSettings::Dark);

        checker[&colorSchemeSpy]++;
        checker[&darkSpy]++;
        checker.checkSpies();

        CHECK(settings->dark());
    }

    SECTION("Setting the same value emits nothing") {
        settings->setColorScheme(cwAppearanceSettings::Dark);
        colorSchemeSpy.clear();
        darkSpy.clear();

        settings->setColorScheme(cwAppearanceSettings::Dark);
        checker.checkSpies();
    }

    SECTION("The value is stored under the colorScheme key") {
        settings->setColorScheme(cwAppearanceSettings::Dark);
        CHECK(diskSettings.value(QStringLiteral("colorScheme")).toInt() == cwAppearanceSettings::Dark);

        settings->setColorScheme(cwAppearanceSettings::Light);
        CHECK(diskSettings.value(QStringLiteral("colorScheme")).toInt() == cwAppearanceSettings::Light);
    }

    SECTION("Setting Light makes dark() false") {
        settings->setColorScheme(cwAppearanceSettings::Dark);
        REQUIRE(settings->dark());

        settings->setColorScheme(cwAppearanceSettings::Light);
        CHECK_FALSE(settings->dark());
    }

    settings->setColorScheme(cwAppearanceSettings::System);
    CHECK(diskSettings.value(QStringLiteral("colorScheme")).toInt() == cwAppearanceSettings::System);
}
