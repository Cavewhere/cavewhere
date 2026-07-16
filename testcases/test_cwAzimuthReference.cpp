/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch2 includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

//Our includes
#include "cwAzimuthReference.h"
#include "cwCoordinateTransform.h"
#include "cwDeclination.h"
#include "cwGeoPoint.h"
#include "cwGridConvergence.h"
#include "cwMath.h"

//Qt includes
#include <QDateTime>
#include <QTimeZone>

using Catch::Matchers::WithinAbs;
using cwAzimuthReference::Reference;

namespace {
    const QString Wgs84 = QStringLiteral("EPSG:4326");
    const QString Utm13N = QStringLiteral("EPSG:32613"); // central meridian -105°

    // A point well east of the central meridian so convergence is clearly
    // non-zero and positive (grid north east of true north).
    cwGeoPoint boulderUtm()
    {
        cwCoordinateTransform geoToUtm(Wgs84, Utm13N);
        REQUIRE(geoToUtm.isValid());
        return geoToUtm.transform(cwGeoPoint(-104.0, 40.015, 1655.0));
    }

    QDateTime surveyDate()
    {
        return QDateTime(QDate(2026, 6, 20), QTime(0, 0), QTimeZone::UTC);
    }
}

TEST_CASE("cwAzimuthReference: Grid passes the azimuth through and never fails",
          "[cwAzimuthReference]")
{
    // Grid needs no CRS or date — even with an empty CS it succeeds.
    auto result = cwAzimuthReference::resolve(123.4, Reference::Grid,
                                              cwGeoPoint(), QString(), QDateTime());
    REQUIRE(result.available);
    CHECK_THAT(result.azimuth, WithinAbs(123.4, 1e-9));
    CHECK(result.convergence == 0.0);
    CHECK(result.declination == 0.0);
    CHECK(result.reason.isEmpty());
}

TEST_CASE("cwAzimuthReference: Grid wraps out-of-range input into [0,360)",
          "[cwAzimuthReference]")
{
    CHECK_THAT(cwAzimuthReference::resolve(-10.0, Reference::Grid, cwGeoPoint(),
                                           QString(), QDateTime()).azimuth,
               WithinAbs(350.0, 1e-9));
    CHECK_THAT(cwAzimuthReference::resolve(370.0, Reference::Grid, cwGeoPoint(),
                                           QString(), QDateTime()).azimuth,
               WithinAbs(10.0, 1e-9));
}

TEST_CASE("cwAzimuthReference: True applies the grid convergence",
          "[cwAzimuthReference]")
{
    const cwGeoPoint p = boulderUtm();
    const double gridAzimuth = 90.0;

    // The resolver must compose exactly the convergence cwGridConvergence reports.
    auto convergence = cwGridConvergence::computeAt(p, Utm13N);
    REQUIRE_FALSE(convergence.hasError());
    REQUIRE(convergence.value() > 0.0); // east of the central meridian

    auto result = cwAzimuthReference::resolve(gridAzimuth, Reference::True, p,
                                              Utm13N, QDateTime());
    REQUIRE(result.available);
    CHECK_THAT(result.convergence, WithinAbs(convergence.value(), 1e-9));
    CHECK_THAT(result.azimuth,
               WithinAbs(cwWrapDegrees360(gridAzimuth + convergence.value()), 1e-9));
    CHECK(result.declination == 0.0);
}

TEST_CASE("cwAzimuthReference: Magnetic applies convergence and subtracts declination",
          "[cwAzimuthReference]")
{
    const cwGeoPoint p = boulderUtm();
    const double gridAzimuth = 200.0;

    auto convergence = cwGridConvergence::computeAt(p, Utm13N);
    auto declination = cwDeclination::compute(p, Utm13N, surveyDate());
    REQUIRE_FALSE(convergence.hasError());
    REQUIRE_FALSE(declination.hasError());

    auto result = cwAzimuthReference::resolve(gridAzimuth, Reference::Magnetic, p,
                                              Utm13N, surveyDate());
    REQUIRE(result.available);
    CHECK_THAT(result.convergence, WithinAbs(convergence.value(), 1e-9));
    CHECK_THAT(result.declination, WithinAbs(declination.value(), 1e-9));
    CHECK_THAT(result.azimuth,
               WithinAbs(cwWrapDegrees360(gridAzimuth + convergence.value() - declination.value()), 1e-9));
}

TEST_CASE("cwAzimuthReference: geographic CRS yields zero convergence so True equals Grid",
          "[cwAzimuthReference]")
{
    const cwGeoPoint geo(-105.27, 40.015, 1655.0);
    auto result = cwAzimuthReference::resolve(45.0, Reference::True, geo, Wgs84,
                                              QDateTime());
    REQUIRE(result.available);
    CHECK(result.convergence == 0.0);
    CHECK_THAT(result.azimuth, WithinAbs(45.0, 1e-9));
}

TEST_CASE("cwAzimuthReference: geographic CRS still resolves Magnetic via declination",
          "[cwAzimuthReference]")
{
    const cwGeoPoint geo(-105.27, 40.015, 1655.0);
    auto declination = cwDeclination::compute(geo, Wgs84, surveyDate());
    REQUIRE_FALSE(declination.hasError());

    auto result = cwAzimuthReference::resolve(45.0, Reference::Magnetic, geo, Wgs84,
                                              surveyDate());
    REQUIRE(result.available);
    CHECK(result.convergence == 0.0);
    CHECK_THAT(result.azimuth, WithinAbs(cwWrapDegrees360(45.0 - declination.value()), 1e-9));
}

TEST_CASE("cwAzimuthReference: True is n/a with an empty coordinate system",
          "[cwAzimuthReference]")
{
    auto result = cwAzimuthReference::resolve(45.0, Reference::True, boulderUtm(),
                                              QString(), QDateTime());
    CHECK_FALSE(result.available);
    CHECK_FALSE(result.reason.isEmpty());
    CHECK(result.azimuth == 0.0);
}

TEST_CASE("cwAzimuthReference: Magnetic is n/a with an unknown coordinate system",
          "[cwAzimuthReference]")
{
    auto result = cwAzimuthReference::resolve(45.0, Reference::Magnetic, boulderUtm(),
                                              QStringLiteral("EPSG:999999"), surveyDate());
    CHECK_FALSE(result.available);
    CHECK_FALSE(result.reason.isEmpty());
}

TEST_CASE("cwAzimuthReference: Magnetic is n/a with an invalid date",
          "[cwAzimuthReference]")
{
    auto result = cwAzimuthReference::resolve(45.0, Reference::Magnetic, boulderUtm(),
                                              Utm13N, QDateTime());
    CHECK_FALSE(result.available);
    CHECK_FALSE(result.reason.isEmpty());
}
