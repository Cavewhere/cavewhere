//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwNoteTranformation.h"
#include "cwLength.h"
#include "cwScale.h"
#include "SpyChecker.h"
#include "cwImage.h"

//Qt includes
#include "cwSignalSpy.h"
#include "cwImageResolution.h"

//Std includes
#include <cmath>
#include <limits>

TEST_CASE("cwNoteTransformation should update correctly", "[cwNoteTransformation]") {

    cwNoteTranformation transform;

    cwSignalSpy scaleSpy(&transform, &cwNoteTranformation::scaleChanged);
    cwSignalSpy northUpSpy(&transform, &cwNoteTranformation::northUpChanged);

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
            CHECK(transform.scale() == Catch::Approx(500.0));
            spyChecker[&scaleSpy]++;
            spyChecker.checkSpies();
        }

        SECTION("Change numerator") {
            transform.scaleNumerator()->setValue(400.0);
            CHECK(transform.scale() == Catch::Approx(400.0));
            spyChecker[&scaleSpy]++;
            spyChecker.checkSpies();
        }

        SECTION("Change denominator") {
            transform.scaleDenominator()->setValue(500.0);
            CHECK(transform.scale() == Catch::Approx(1 / 500.0));
            spyChecker[&scaleSpy]++;
            spyChecker.checkSpies();
        }

        SECTION("Change scale") {
            transform.scaleObject()->setScale(250);
            CHECK(transform.scale() == Catch::Approx(250.0));
            spyChecker[&scaleSpy]++;
            spyChecker.checkSpies();
        }

        SECTION("Change units") {
            transform.scaleNumerator()->setValue(2.0);
            transform.scaleDenominator()->setValue(80.0);
            transform.scaleNumerator()->setUnit(cwUnits::Centimeters);
            transform.scaleDenominator()->setUnit(cwUnits::Meters);
            CHECK(transform.scale() == Catch::Approx(0.02 / 80.0));
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

TEST_CASE("cwNoteTransformation scale calculation handles invalid inputs", "[cwNoteTransformation]") {
    cwNoteTranformation transform;
    cwLength length;
    cwImageResolution resolution;

    length.setValue(1.0);
    length.setUnit(cwUnits::Meters);

    resolution.setValue(300.0);
    resolution.setUnit(cwUnits::DotsPerInch);

    const QSize imageSize(1000, 1000);

    SECTION("Valid inputs produce a finite, positive scale") {
        const double scale = transform.calculateScale(QPointF(0.0, 0.0),
                                                      QPointF(1.0, 0.0),
                                                      &length,
                                                      imageSize,
                                                      &resolution);
        CHECK(std::isfinite(scale));
        CHECK(scale > 0.0);
    }

    SECTION("Invalid resolution produces NaN scale") {
        resolution.setValue(std::numeric_limits<double>::infinity());
        const double scale = transform.calculateScale(QPointF(0.0, 0.0),
                                                      QPointF(1.0, 0.0),
                                                      &length,
                                                      imageSize,
                                                      &resolution);
        CHECK(std::isnan(scale));
    }

    SECTION("Zero length produces NaN scale") {
        length.setValue(0.0);
        const double scale = transform.calculateScale(QPointF(0.0, 0.0),
                                                      QPointF(1.0, 0.0),
                                                      &length,
                                                      imageSize,
                                                      &resolution);
        CHECK(std::isnan(scale));
    }

    SECTION("Degenerate points produce NaN scale") {
        const double scale = transform.calculateScale(QPointF(0.0, 0.0),
                                                      QPointF(0.0, 0.0),
                                                      &length,
                                                      imageSize,
                                                      &resolution);
        CHECK(std::isnan(scale));
    }
}

TEST_CASE("cwNoteTransformation scale calculation uses native units for rendered size", "[cwNoteTransformation]") {
    cwNoteTranformation transform;
    cwLength length;
    cwImageResolution resolution;

    length.setValue(1.0);
    length.setUnit(cwUnits::Meters);

    resolution.setValue(100.0);
    resolution.setUnit(cwUnits::DotsPerInch);

    cwImage image;
    image.setOriginalSize(QSize(2000, 2000));

    const QSize renderedSize(1000, 1000);
    const double scaleRendered = transform.calculateScaleForRendered(QPointF(0.0, 0.0),
                                                                     QPointF(1.0, 0.0),
                                                                     &length,
                                                                     image,
                                                                     renderedSize,
                                                                     &resolution);

    const double dotsPerMeter = resolution.convertTo(cwUnits::DotsPerMeter).value;
    const double expectedScale = 2000.0 / dotsPerMeter;

    CHECK(scaleRendered == Catch::Approx(expectedScale).epsilon(1e-6));
}
