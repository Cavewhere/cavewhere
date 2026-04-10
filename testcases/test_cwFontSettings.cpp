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

    // Reset to known state before each section (singleton survives across sections)
    settings->setFontFamily("Yanone Kaffeesatz");
    settings->setFontBaseSize(16);

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
        settings->setFontFamily("Fira Sans");

        checker[&familySpy]++;
        checker[&baseSizeSpy]++; // size adjusts from 16 to 14
        checker.checkSpies();

        CHECK(settings->fontFamily() == "Fira Sans");
        CHECK(diskSettings.value("fontFamily").toString() == "Fira Sans");
    }

    SECTION("setFontFamily with same value emits no signal") {
        settings->setFontFamily(settings->fontFamily());
        checker.checkSpies();
    }

    SECTION("defaultFontBaseSize varies by font family") {
        CHECK(settings->defaultFontBaseSize() == 16); // Yanone Kaffeesatz
        settings->setFontFamily("Fira Sans");
        CHECK(settings->defaultFontBaseSize() == 14);
        settings->setFontFamily("");
        CHECK(settings->defaultFontBaseSize() == 14); // System
    }

    SECTION("switching font preserves relative offset") {
        // Start at CaveWhere default (16) + 4 = 20
        settings->setFontBaseSize(20);
        settings->setFontFamily("Fira Sans");
        // Fira Sans default (14) + 4 = 18
        CHECK(settings->fontBaseSize() == 18);
    }

    SECTION("switching font at default lands on new default") {
        // At CaveWhere default (16)
        CHECK(settings->fontBaseSize() == 16);
        settings->setFontFamily("Fira Sans");
        CHECK(settings->fontBaseSize() == 14);
    }

    SECTION("switching font clamps to minimum") {
        // Set to CaveWhere default - 6 = 10 (min)
        settings->setFontBaseSize(10);
        // Switch to Fira Sans: default 14, offset = 10-16 = -6, new = 14 + (-6) = 8 → clamped to min
        settings->setFontFamily("Fira Sans");
        CHECK(settings->fontBaseSize() == cwFontSettings::MinFontBaseSize);
    }

    SECTION("min and max font size constants") {
        CHECK(settings->minFontBaseSize() == 10);
        CHECK(settings->maxFontBaseSize() == 28);
    }

    SECTION("fontEntries returns all font options") {
        auto entries = cwFontSettings::fontEntries();
        REQUIRE(entries.size() == 3);
        CHECK(entries[0].label == "CaveWhere");
        CHECK(entries[0].family == "Yanone Kaffeesatz");
        CHECK(entries[0].defaultSize == 16);
        CHECK(entries[1].label == "Fira Sans");
        CHECK(entries[1].family == "Fira Sans");
        CHECK(entries[1].defaultSize == 14);
        CHECK(entries[2].label == "System");
        CHECK(entries[2].family == "");
        CHECK(entries[2].defaultSize == 14);
    }

    diskSettings.remove("fontBaseSize");
    diskSettings.remove("fontFamily");
}
