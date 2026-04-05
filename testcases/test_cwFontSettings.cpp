//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwFontSettings.h"
#include "cwSignalSpy.h"
#include "SpyChecker.h"

//Qt includes
#include <QSettings>

TEST_CASE("cwFontSettings should initialize correctly and persist settings", "[cwFontSettings]") {
    // Clear any persisted state from a previous run
    QSettings diskSettings;
    diskSettings.remove("fontBaseSize");
    diskSettings.remove("fontFamily");

    cwFontSettings::initialize();
    auto settings = cwFontSettings::instance();

    // initialize() is idempotent — second call returns the same instance
    cwFontSettings::initialize();
    CHECK(cwFontSettings::instance() == settings);
    REQUIRE(settings);

    cwSignalSpy baseSizeSpy(settings, &cwFontSettings::fontBaseSizeChanged);
    cwSignalSpy familySpy(settings, &cwFontSettings::fontFamilyChanged);

    baseSizeSpy.setObjectName("baseSizeSpy");
    familySpy.setObjectName("familySpy");

    SpyChecker checker = {
        {&baseSizeSpy, 0},
        {&familySpy,   0}
    };

    SECTION("Default values") {
        CHECK(settings->fontBaseSize() == settings->defaultFontBaseSize());
        CHECK(settings->defaultFontBaseSize() == 16);
        CHECK(settings->fontFamily() == "Yanone Kaffeesatz");
    }

    SECTION("setFontBaseSize persists and signals") {
        settings->setFontBaseSize(18);

        checker[&baseSizeSpy]++;
        checker.checkSpies();

        CHECK(settings->fontBaseSize() == 18);
        CHECK(diskSettings.value("fontBaseSize").toInt() == 18);
    }

    SECTION("setFontBaseSize with same value emits no signal") {
        settings->setFontBaseSize(settings->fontBaseSize());
        checker.checkSpies();
    }

    SECTION("setFontFamily persists and signals") {
        settings->setFontFamily("Bitstream Vera Sans");

        checker[&familySpy]++;
        checker.checkSpies();

        CHECK(settings->fontFamily() == "Bitstream Vera Sans");
        CHECK(diskSettings.value("fontFamily").toString() == "Bitstream Vera Sans");
    }

    SECTION("setFontFamily with same value emits no signal") {
        settings->setFontFamily(settings->fontFamily());
        checker.checkSpies();
    }

    SECTION("defaultFontBaseSize is constant") {
        CHECK(settings->defaultFontBaseSize() == 16);
        settings->setFontBaseSize(20);
        CHECK(settings->defaultFontBaseSize() == 16);
    }

    diskSettings.remove("fontBaseSize");
    diskSettings.remove("fontFamily");
}
