//Catch includes
#include <catch2/catch.hpp>

//Our includes
#include "cwNoteTranformation.h"
#include "cwLength.h"
#include "cwScale.h"
#include "SpyChecker.h"

//Qt includes
#include <QSignalSpy>

TEST_CASE("cwNoteTransformation should update correctly", "[cwNoteTransformation]") {

    cwNoteTranformation transform;

    QSignalSpy scaleSpy(&transform, &cwNoteTranformation::scaleChanged);
    QSignalSpy northUpSpy(&transform, &cwNoteTranformation::northUpChanged);

    scaleSpy.setObjectName("scaleSpy");
    northUpSpy.setObjectName("northUpSpy");

    SpyChecker spyChecker = {
        {&scaleSpy, 0},
        {&northUpSpy, 0}
    };

    CHECK(transform.scale() == 1.0);
    CHECK(transform.northUp() == 0.0);
    CHECK(transform.scaleNumerator()->value() == 1.0);
    CHECK(transform.scaleDenominator()->value() == 1.0);
    CHECK(transform.scaleObject()->scale() == 1.0);
    CHECK(transform.scaleObject()->scaleNumerator()->value() == 1.0);
    CHECK(transform.scaleObject()->scaleDenominator()->value() == 1.0);
    CHECK(transform.scaleObject()->scaleNumerator()->unit() == 0);
    CHECK(transform.scaleObject()->scaleDenominator()->unit() == 0);

    spyChecker.checkSpies();

    SECTION("Check that scale updating works") {
        SECTION("Change scale directly") {
            transform.setScale(500.0);
            CHECK(transform.scale() == Approx(500.0));
            spyChecker[&scaleSpy]++;
            spyChecker.checkSpies();
        }

        SECTION("Change numerator") {
            transform.scaleNumerator()->setValue(400.0);
            CHECK(transform.scale() == Approx(400.0));
            spyChecker[&scaleSpy]++;
            spyChecker.checkSpies();
        }

        SECTION("Change denominator") {
            transform.scaleDenominator()->setValue(500.0);
            CHECK(transform.scale() == Approx(1 / 500.0));
            spyChecker[&scaleSpy]++;
            spyChecker.checkSpies();
        }

        SECTION("Change scale") {
            transform.scaleObject()->setScale(250);
            CHECK(transform.scale() == Approx(250.0));
            spyChecker[&scaleSpy]++;
            spyChecker.checkSpies();
        }

        SECTION("Change units") {
            transform.scaleNumerator()->setValue(2.0);
            transform.scaleDenominator()->setValue(80.0);
            transform.scaleNumerator()->setUnit(cwUnits::Centimeters);
            transform.scaleDenominator()->setUnit(cwUnits::Meters);
            CHECK(transform.scale() == Approx(0.02 / 80.0));
            spyChecker[&scaleSpy] += 4;
            spyChecker.checkSpies();
        }
    }

    SECTION("Check that north up works") {
        transform.setNorthUp(10.0);
        spyChecker[&northUpSpy]++;
        spyChecker.checkSpies();
    }
}
