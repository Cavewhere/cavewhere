/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwSketchSettings.h"

// Qt
#include <QSettings>
#include <QUuid>

TEST_CASE("cwSketchSettings persists defaultPaletteId through QSettings",
          "[cwSketchSettings]")
{
    cwSketchSettings::initialize();
    auto *settings = cwSketchSettings::instance();
    REQUIRE(settings);

    const QUuid original = settings->defaultPaletteId();

    const QUuid id = QUuid::createUuid();
    settings->setDefaultPaletteId(id);
    CHECK(settings->defaultPaletteId() == id);

    // It reaches the backing store, so the next launch reads it back.
    {
        QSettings store;
        CHECK(QUuid(store.value(QStringLiteral("sketch/defaultPaletteId")).toString()) == id);
    }

    // Null clears the key rather than persisting an empty UUID.
    settings->setDefaultPaletteId(QUuid());
    CHECK(settings->defaultPaletteId().isNull());
    {
        QSettings store;
        CHECK_FALSE(store.contains(QStringLiteral("sketch/defaultPaletteId")));
    }

    settings->setDefaultPaletteId(original);
}
