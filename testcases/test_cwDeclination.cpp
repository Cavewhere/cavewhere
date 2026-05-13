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
#include "cwCoordinateTransform.h"
#include "cwDeclination.h"
#include "cwGeoPoint.h"

//Qt includes
#include <QDateTime>
#include <QTimeZone>
#include <QtMath>

using Catch::Matchers::WithinAbs;

namespace {
    // Boulder, CO — picked because magnetic declination there is well-known
    // (~+8° east in the early 2020s) and IGRF's secular variation is large
    // enough at this latitude that a 45-year decade-drift test is robust.
    const cwGeoPoint BoulderWgs84(-105.27, 40.015, 1655.0);
    const QString Wgs84 = QStringLiteral("EPSG:4326");
    const QString Utm13N = QStringLiteral("EPSG:32613");

    QDateTime makeDate(int year, int month, int day)
    {
        return QDateTime(QDate(year, month, day), QTime(0, 0), QTimeZone::UTC);
    }
}

TEST_CASE("cwDeclination: WGS84 geographic input near Boulder, CO", "[cwDeclination]")
{
    auto result = cwDeclination::compute(BoulderWgs84, Wgs84, makeDate(2024, 1, 1));
    REQUIRE_FALSE(result.hasError());
    // Loose bounds — exact value tracks whichever IGRF version survex ships.
    CHECK(result.value() > 5.0);
    CHECK(result.value() < 11.0);
}

TEST_CASE("cwDeclination: UTM input transforms to lat/lon and gives same value as direct geographic", "[cwDeclination]")
{
    cwCoordinateTransform geoToUtm(Wgs84, Utm13N);
    REQUIRE(geoToUtm.isValid());
    const cwGeoPoint utm = geoToUtm.transform(BoulderWgs84);

    const QDateTime date = makeDate(2024, 6, 15);
    auto geoResult = cwDeclination::compute(BoulderWgs84, Wgs84, date);
    auto utmResult = cwDeclination::compute(utm, Utm13N, date);
    REQUIRE_FALSE(geoResult.hasError());
    REQUIRE_FALSE(utmResult.hasError());

    CHECK_THAT(utmResult.value(), WithinAbs(geoResult.value(), 0.01));
}

TEST_CASE("cwDeclination: declination drifts measurably across decades", "[cwDeclination]")
{
    auto early = cwDeclination::compute(BoulderWgs84, Wgs84, makeDate(1980, 1, 1));
    auto late  = cwDeclination::compute(BoulderWgs84, Wgs84, makeDate(2025, 1, 1));
    REQUIRE_FALSE(early.hasError());
    REQUIRE_FALSE(late.hasError());
    CHECK(qAbs(early.value() - late.value()) > 1.0);
}

TEST_CASE("cwDeclination: invalid date returns error", "[cwDeclination]")
{
    auto result = cwDeclination::compute(BoulderWgs84, Wgs84, QDateTime());
    CHECK(result.hasError());
    CHECK(result.errorMessage().contains("date", Qt::CaseInsensitive));
}

TEST_CASE("cwDeclination: empty source CS returns error", "[cwDeclination]")
{
    auto result = cwDeclination::compute(BoulderWgs84, QString(), makeDate(2024, 1, 1));
    CHECK(result.hasError());
    CHECK(result.errorMessage().contains("coordinate", Qt::CaseInsensitive));
}

TEST_CASE("cwDeclination: unknown source CS returns transform error", "[cwDeclination]")
{
    auto result = cwDeclination::compute(cwGeoPoint(0.0, 0.0, 0.0),
                                          QStringLiteral("EPSG:999999"),
                                          makeDate(2024, 1, 1));
    CHECK(result.hasError());
    CHECK(result.errorMessage().contains("transform", Qt::CaseInsensitive));
}
