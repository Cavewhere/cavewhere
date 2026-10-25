//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
using namespace Catch;

//Qt includes
#include <QColor>

//Our includes
#include "cwEDLSettings.h"
#include "cwEdlParametersData.h"
#include "cwSignalSpy.h"
#include "SpyChecker.h"

TEST_CASE("cwEDLSettings exposes the shared EDL defaults", "[cwEDLSettings]")
{
    cwEDLSettings settings;
    const EdlParametersData defaults;

    CHECK(settings.strength() == Approx(defaults.strength));
    CHECK(settings.maxDarken() == Approx(defaults.maxDarken));
    CHECK(settings.radius() == Approx(defaults.radiusPx));
    CHECK(settings.outlineColor() == QColor(Qt::black));

    const EdlParametersData params = settings.parameters();
    CHECK(params.strength == Approx(defaults.strength));
    CHECK(params.maxDarken == Approx(defaults.maxDarken));
    CHECK(params.radiusPx == Approx(defaults.radiusPx));
    CHECK(params.outlineColor == QColor(Qt::black));
}

TEST_CASE("cwEDLSettings setters update values and emit signals", "[cwEDLSettings]")
{
    cwEDLSettings settings;

    cwSignalSpy strengthSpy(&settings, &cwEDLSettings::strengthChanged);
    cwSignalSpy maxDarkenSpy(&settings, &cwEDLSettings::maxDarkenChanged);
    cwSignalSpy radiusSpy(&settings, &cwEDLSettings::radiusChanged);
    cwSignalSpy outlineColorSpy(&settings, &cwEDLSettings::outlineColorChanged);
    cwSignalSpy changedSpy(&settings, &cwEDLSettings::changed);

    strengthSpy.setObjectName("strengthSpy");
    maxDarkenSpy.setObjectName("maxDarkenSpy");
    radiusSpy.setObjectName("radiusSpy");
    outlineColorSpy.setObjectName("outlineColorSpy");
    changedSpy.setObjectName("changedSpy");

    SpyChecker checker = {
        {&strengthSpy, 0},
        {&maxDarkenSpy, 0},
        {&radiusSpy, 0},
        {&outlineColorSpy, 0},
        {&changedSpy, 0},
    };

    SECTION("setStrength updates value, parameters, and fires strength + changed") {
        settings.setStrength(2500.0f);
        checker[&strengthSpy]++;
        checker[&changedSpy]++;
        checker.checkSpies();

        CHECK(settings.strength() == Approx(2500.0f));
        CHECK(settings.parameters().strength == Approx(2500.0f));
    }

    SECTION("setMaxDarken updates value, parameters, and fires maxDarken + changed") {
        settings.setMaxDarken(12.5f);
        checker[&maxDarkenSpy]++;
        checker[&changedSpy]++;
        checker.checkSpies();

        CHECK(settings.maxDarken() == Approx(12.5f));
        CHECK(settings.parameters().maxDarken == Approx(12.5f));
    }

    SECTION("setRadius updates value, parameters, and fires radius + changed") {
        settings.setRadius(4.0f);
        checker[&radiusSpy]++;
        checker[&changedSpy]++;
        checker.checkSpies();

        CHECK(settings.radius() == Approx(4.0f));
        CHECK(settings.parameters().radiusPx == Approx(4.0f));
    }

    SECTION("setOutlineColor updates value, parameters, and fires outlineColor + changed") {
        const QColor gray(QStringLiteral("#8a96a3"));
        settings.setOutlineColor(gray);
        checker[&outlineColorSpy]++;
        checker[&changedSpy]++;
        checker.checkSpies();

        CHECK(settings.outlineColor() == gray);
        CHECK(settings.parameters().outlineColor == gray);

        settings.setOutlineColor(gray);
        checker.checkSpies();
    }

    SECTION("setting the current value is a no-op and emits nothing") {
        settings.setStrength(settings.strength());
        settings.setMaxDarken(settings.maxDarken());
        settings.setRadius(settings.radius());
        settings.setOutlineColor(settings.outlineColor());
        checker.checkSpies();
    }
}
