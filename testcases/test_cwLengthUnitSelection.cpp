/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLengthUnitSelection.h"
#include "cwUnits.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Qt includes
#include <QSettings>
#include <QStringList>

using namespace Catch;

TEST_CASE("cwLengthUnitSelection exposes the curated set", "[cwLengthUnitSelection]")
{
    cwLengthUnitSelection selection;

    SECTION("names are the curated set in menu order") {
        CHECK(selection.names()
              == QStringList({QStringLiteral("m"), QStringLiteral("km"),
                              QStringLiteral("ft"), QStringLiteral("mi")}));
        // The static accessor is the same single source of truth.
        CHECK(cwLengthUnitSelection::unitNames() == selection.names());
    }

    SECTION("defaults to metres at index 0") {
        CHECK(selection.unit() == cwUnits::Meters);
        CHECK(selection.index() == 0);
        CHECK(selection.name() == QStringLiteral("m"));
    }

    SECTION("index maps to the curated unit both ways") {
        selection.setIndex(2);
        CHECK(selection.unit() == cwUnits::Feet);
        CHECK(selection.index() == 2);
        CHECK(selection.name() == QStringLiteral("ft"));

        selection.setUnit(cwUnits::Miles);
        CHECK(selection.index() == 3);
    }

    SECTION("out-of-range indices are ignored") {
        selection.setIndex(2);
        selection.setIndex(-1);
        selection.setIndex(99);
        CHECK(selection.unit() == cwUnits::Feet);
    }

    SECTION("an unlisted unit coerces to metres") {
        // Inches isn't in the curated set, so it can't be selected.
        selection.setUnit(cwUnits::Inches);
        CHECK(selection.unit() == cwUnits::Meters);
    }

    SECTION("converts to and from the selected unit") {
        selection.setUnit(cwUnits::Feet);
        CHECK(selection.fromMeters(60.0) == Approx(196.8503937));
        CHECK(selection.toMeters(196.8503937) == Approx(60.0));
    }
}

TEST_CASE("cwLengthUnitSelection persists to its settings key",
          "[cwLengthUnitSelection]")
{
    // Each test process has a PID-scoped QSettings, cleared at startup; clear
    // again so this case is independent of sibling cases.
    QSettings().clear();
    const QString key = QStringLiteral("test/lengthUnit");

    SECTION("setting the key loads a previously stored unit") {
        {
            cwLengthUnitSelection writer;
            writer.setSettingsKey(key);
            writer.setUnit(cwUnits::Feet);
        }

        cwLengthUnitSelection reader;
        reader.setSettingsKey(key);
        CHECK(reader.unit() == cwUnits::Feet);
    }

    SECTION("without a key nothing is persisted") {
        cwLengthUnitSelection noKey;
        noKey.setUnit(cwUnits::Feet);

        cwLengthUnitSelection keyed;
        keyed.setSettingsKey(key);
        CHECK(keyed.unit() == cwUnits::Meters);
    }

    SECTION("a corrupt stored value falls back to metres") {
        QSettings().setValue(key, 999);
        cwLengthUnitSelection selection;
        selection.setSettingsKey(key);
        CHECK(selection.unit() == cwUnits::Meters);
    }

    QSettings().clear();
}
