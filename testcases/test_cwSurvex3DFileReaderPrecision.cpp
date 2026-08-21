/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Guards the frame cavern's .3d output is read into. The reader narrows to
// QVector3D, so the coordinates it is handed have to be small enough for
// float32 to hold millimetres - which is why the export names the project's
// local projection in *cs out. At full UTM magnitude a float32 northing snaps
// onto a half-metre grid, bending shot bearings by degrees and stretching
// steep legs by several percent; these solve at local-projection distances
// from the anchor, where a ULP is under a micrometre.

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

    // A fix at the far edge of what the local projection reaches (~50 km from
    // the anchor), which is the largest coordinate the reader can be handed.
    // Whole metres, so cavern's centimetre .3d quantization lands on the same
    // grid however the survey is fixed.
    const cwGeoPoint kFix(34000.0, 41000.0, 330.0);

    // A steep leg first: only 3.90m of the 9.77m tape is horizontal, so a fixed
    // per-station error eats a much larger fraction of the plan-view shot than
    // it would on a level leg. That is the case that surfaced the bug.
    constexpr double kSteepTape = 9.77;
    constexpr double kSteepCompass = 21.5;
    constexpr double kSteepClino = -66.5;

    // Solves the same three shots fixed at \a origin, so every solve is the same
    // survey in a different place. \a dir owns the .svx, the .3d and cavern's
    // sidecars, so concurrent test processes never share a path.
    cwSurvex3DFileReader::NetworkAndLookup solve(const QTemporaryDir& dir,
                                                 const cwGeoPoint& origin,
                                                 const QString& name = QStringLiteral("survey"))
    {
        const QString svxPath = dir.filePath(name + QStringLiteral(".svx"));
        const QString output3dPath = dir.filePath(name + QStringLiteral(".3d"));

        QFile svx(svxPath);
        REQUIRE(svx.open(QIODevice::WriteOnly));
        svx.write(QStringLiteral("*fix a0 %1 %2 %3\n")
                      .arg(origin.x, 0, 'f', 2)
                      .arg(origin.y, 0, 'f', 2)
                      .arg(origin.z, 0, 'f', 2)
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
        auto parsed = reader.readNetworkAndLookup(cavernResult.value().output3dPath);
        REQUIRE_FALSE(parsed.lookup.isEmpty());
        return parsed;
    }
}

TEST_CASE("Moving a survey across the frame doesn't change its shape",
          "[cwSurvex3DFileReaderPrecision]") {
    // The same survey must solve to the same shape whether it sits on the
    // frame's anchor or at the far edge of its reach. Both reads narrow to
    // float32, and at these magnitudes a float ULP is under a micrometre - so
    // this compares the shapes exactly, not to a tolerance.
    constexpr double kExactMargin = 1e-4;

    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    const auto atAnchor = solve(dir, cwGeoPoint(), QStringLiteral("anchor"));
    const auto atEdge = solve(dir, kFix, QStringLiteral("edge"));

    const QMap<QString, QVector3D> anchorPositions = atAnchor.lookup.positions();
    REQUIRE(anchorPositions.size() == 4);
    REQUIRE(atEdge.lookup.positions().size() == anchorPositions.size());

    const QVector3D offset = kFix.toVector3D();
    for(auto it = anchorPositions.constBegin(); it != anchorPositions.constEnd(); ++it) {
        INFO("Station: " << it.key().toStdString());
        REQUIRE(atEdge.lookup.hasPosition(it.key()));

        const QVector3D expected = it.value() + offset;
        const QVector3D actual = atEdge.lookup.position(it.key());
        CHECK(actual.x() == Catch::Approx(expected.x()).margin(kExactMargin));
        CHECK(actual.y() == Catch::Approx(expected.y()).margin(kExactMargin));
        CHECK(actual.z() == Catch::Approx(expected.z()).margin(kExactMargin));
    }
}

TEST_CASE("A steep leg out at the frame's edge keeps its bearing and foreshortening",
          "[cwSurvex3DFileReaderPrecision]") {
    // The user-visible symptom of losing this precision: shot leaders drawn on
    // a note missed the station they named, because the plotted leg was both
    // rotated off its compass bearing and longer than tape x cos(clino).
    //
    // The .3d format stores coordinates as int32 centimetres (img.c
    // read_coord), so two independently rounded endpoints can differ from the
    // ideal reduction by up to ~1.5cm.
    constexpr double kCentimetreMargin = 0.02;

    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    const auto atEdge = solve(dir, kFix);

    REQUIRE(atEdge.lookup.hasPosition(QStringLiteral("a0")));
    REQUIRE(atEdge.lookup.hasPosition(QStringLiteral("a1")));

    const QVector3D leg = atEdge.lookup.position(QStringLiteral("a1"))
                          - atEdge.lookup.position(QStringLiteral("a0"));

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

TEST_CASE("The survey network shares the lookup's frame",
          "[cwSurvex3DFileReaderPrecision]") {
    // cwLinePlotTask publishes the reader's network region-wide (it drives
    // cwSurvey2DGeometryRule and cwCaptureCenterline) while the lookup drives
    // the scraps and the centerline geometry. One pass over the .3d fills both,
    // so they can never disagree about where a station is.

    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    const auto atEdge = solve(dir, kFix);

    const QStringList stations = atEdge.network.stations();
    REQUIRE(stations.size() == 4);

    for(const QString& station : stations) {
        INFO("Station: " << station.toStdString());
        REQUIRE(atEdge.lookup.hasPosition(station));
        CHECK(atEdge.network.position(station) == atEdge.lookup.position(station));
    }
}
