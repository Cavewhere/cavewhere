/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Guards the frame cavern's .3d output is read into. Coordinates arrive at
// full UTM magnitude for a georeferenced cave, and narrowing those to
// QVector3D before subtracting worldOrigin snapped every northing onto a
// half-metre grid - a float32 ULP at 5.47e6m - which bent shot bearings by
// degrees and stretched steep legs by several percent.

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwCavernRunner.h"
#include "cwGeoPoint.h"
#include "cwStationPositionLookup.h"
#include "cwSurvex3DFileReader.h"
#include "cwSurveyNetwork.h"

//Qt includes
#include <QFile>
#include <QTemporaryDir>
#include <QVector3D>
#include <QtMath>

//Std includes
#include <cmath>

namespace {

    // A real georeferenced fix, far enough north that a float32 holding the
    // absolute northing has a 0.5m ULP. Whole metres, so cavern's centimetre
    // .3d quantization lands on the same grid however the survey is fixed.
    const cwGeoPoint kFix(288805.0, 5474178.0, 330.0);

    // A steep leg first: only 3.90m of the 9.77m tape is horizontal, so a fixed
    // per-station error eats a much larger fraction of the plan-view shot than
    // it would on a level leg. That is the case that surfaced the bug.
    constexpr double kSteepTape = 9.77;
    constexpr double kSteepCompass = 21.5;
    constexpr double kSteepClino = -66.5;

    // Solves the same three shots fixed at \a worldOrigin and reads the result
    // back with that origin subtracted, so every solve is the same survey in a
    // different place. \a dir owns the .svx, the .3d and cavern's sidecars, so
    // concurrent test processes never share a path.
    cwSurvex3DFileReader::NetworkAndLookup solve(const QTemporaryDir& dir,
                                                 const cwGeoPoint& worldOrigin,
                                                 const QString& name = QStringLiteral("survey"))
    {
        const QString svxPath = dir.filePath(name + QStringLiteral(".svx"));
        const QString output3dPath = dir.filePath(name + QStringLiteral(".3d"));

        QFile svx(svxPath);
        REQUIRE(svx.open(QIODevice::WriteOnly));
        svx.write(QStringLiteral("*fix a0 %1 %2 %3\n")
                      .arg(worldOrigin.x, 0, 'f', 2)
                      .arg(worldOrigin.y, 0, 'f', 2)
                      .arg(worldOrigin.z, 0, 'f', 2)
                      .toUtf8());
        svx.write("*data normal from to tape compass clino\n");
        svx.write(QStringLiteral("a0 a1 %1 %2 %3\n")
                      .arg(kSteepTape).arg(kSteepCompass).arg(kSteepClino)
                      .toUtf8());
        // Two more legs so the network has interior stations to resolve.
        svx.write("a1 a2 15.47 190.0 -50.0\n"
                  "a2 a3 4.2 300.0 12.0\n");
        svx.close();

        const auto cavernResult = cwCavernRunner::run(svxPath, output3dPath);
        REQUIRE_FALSE(cavernResult.hasError());

        cwSurvex3DFileReader reader;
        auto parsed = reader.readNetworkAndLookup(cavernResult.value().output3dPath, worldOrigin);
        REQUIRE_FALSE(parsed.lookup.isEmpty());
        return parsed;
    }
}

TEST_CASE("Georeferencing a survey doesn't change its shape",
          "[cwSurvex3DFileReaderPrecision]") {
    // The same survey must solve to the same shape whether it is fixed at the
    // origin or out in UTM. Both reads narrow to float32 once the origin is
    // off, at which point the stations sit within ~20m of zero and a float ULP
    // is under a micrometre - so this compares exactly, not to a tolerance.
    constexpr double kExactMargin = 1e-4;

    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    const auto local = solve(dir, cwGeoPoint(), QStringLiteral("local"));
    const auto geo = solve(dir, kFix, QStringLiteral("geo"));

    const QMap<QString, QVector3D> localPositions = local.lookup.positions();
    REQUIRE(localPositions.size() == 4);
    REQUIRE(geo.lookup.positions().size() == localPositions.size());

    for(auto it = localPositions.constBegin(); it != localPositions.constEnd(); ++it) {
        INFO("Station: " << it.key().toStdString());
        REQUIRE(geo.lookup.hasPosition(it.key()));

        const QVector3D expected = it.value();
        const QVector3D actual = geo.lookup.position(it.key());
        CHECK(actual.x() == Catch::Approx(expected.x()).margin(kExactMargin));
        CHECK(actual.y() == Catch::Approx(expected.y()).margin(kExactMargin));
        CHECK(actual.z() == Catch::Approx(expected.z()).margin(kExactMargin));
    }
}

TEST_CASE("A georeferenced steep leg keeps its bearing and foreshortening",
          "[cwSurvex3DFileReaderPrecision]") {
    // The user-visible symptom: shot leaders drawn on a note missed the station
    // they named, because the plotted leg was both rotated off its compass
    // bearing and longer than tape x cos(clino).
    //
    // The .3d format stores coordinates as int32 centimetres (img.c
    // read_coord), so two independently rounded endpoints can differ from the
    // ideal reduction by up to ~1.5cm.
    constexpr double kCentimetreMargin = 0.02;

    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    const auto geo = solve(dir, kFix);

    REQUIRE(geo.lookup.hasPosition(QStringLiteral("a0")));
    REQUIRE(geo.lookup.hasPosition(QStringLiteral("a1")));

    const QVector3D leg = geo.lookup.position(QStringLiteral("a1"))
                          - geo.lookup.position(QStringLiteral("a0"));

    const double clinoRadians = qDegreesToRadians(kSteepClino);
    const double compassRadians = qDegreesToRadians(kSteepCompass);
    const double horizontal = kSteepTape * std::cos(clinoRadians);

    CHECK(double(leg.x()) == Catch::Approx(horizontal * std::sin(compassRadians))
                                 .margin(kCentimetreMargin));
    CHECK(double(leg.y()) == Catch::Approx(horizontal * std::cos(compassRadians))
                                 .margin(kCentimetreMargin));
    CHECK(double(leg.z()) == Catch::Approx(kSteepTape * std::sin(clinoRadians))
                                 .margin(kCentimetreMargin));
}

TEST_CASE("The survey network shares the lookup's origin-relative frame",
          "[cwSurvex3DFileReaderPrecision]") {
    // cwLinePlotTask publishes the reader's network region-wide (it drives
    // cwSurvey2DGeometryRule and cwCaptureCenterline) while the lookup drives
    // the scraps and the centerline geometry. The offset used to be applied to
    // the lookup alone, leaving the two in frames a whole UTM apart.

    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    const auto geo = solve(dir, kFix);

    const QStringList stations = geo.network.stations();
    REQUIRE(stations.size() == 4);

    for(const QString& station : stations) {
        INFO("Station: " << station.toStdString());
        REQUIRE(geo.lookup.hasPosition(station));
        CHECK(geo.network.position(station) == geo.lookup.position(station));

        // Origin-relative, so every station is a few metres from zero rather
        // than a few million.
        CHECK(geo.lookup.position(station).length() < 100.0f);
    }
}
