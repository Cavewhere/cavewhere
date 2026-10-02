/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Cavewhere includes
#include "cwSurvexExporter.h"
#include "cwSurvexExporterUtils.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwDistanceReading.h"

//Qt includes
#include <QString>
#include <QStringList>
#include <QTextStream>

namespace {

cwShot shotWithReadings() {
    cwShot shot;
    shot.setDistance(QStringLiteral("5.0"));
    shot.setCompass(QStringLiteral("10"));
    shot.setClino(QStringLiteral("0"));
    return shot;
}

cwStation station(const QString& name, bool withLrud) {
    cwStation station(name);
    if(withLrud) {
        station.setLeft(cwDistanceReading(QStringLiteral("1")));
        station.setRight(cwDistanceReading(QStringLiteral("2")));
        station.setUp(cwDistanceReading(QStringLiteral("3")));
        station.setDown(cwDistanceReading(QStringLiteral("4")));
    }
    return station;
}

//The i-th chunk gets one shot whose from-station carries LRUDs when chunkHasLrud.at(i)
QString exportTrip(const QList<bool>& chunkHasLrud) {
    cwTripData trip;
    trip.calibrations.setFrontSights(true);
    trip.calibrations.setBackSights(false);

    for(int i = 0; i < chunkHasLrud.size(); i++) {
        cwSurveyChunkData chunk;
        chunk.stations.append(station(QStringLiteral("a%1").arg(i * 2 + 1), chunkHasLrud.at(i)));
        chunk.stations.append(station(QStringLiteral("a%1").arg(i * 2 + 2), false));
        chunk.shots.append(shotWithReadings());
        trip.chunks.append(chunk);
    }

    QString output;
    QTextStream stream(&output);
    QStringList errors;
    cwSurvexExporter::writeTrip(stream, trip, errors);
    REQUIRE(errors.isEmpty());
    return output;
}

//! The line following the first passage header, or a null string when there is no header
QString firstPassageRow(const QString& output) {
    const QStringList lines = output.split('\n');
    const qsizetype headerIndex = lines.indexOf(cwSurvexExporterUtils::passageDataHeader());
    if(headerIndex < 0 || headerIndex + 1 >= lines.size()) { return QString(); }
    return lines.at(headerIndex + 1).simplified();
}

} // namespace

TEST_CASE("Survex trip export writes no *data passage block for chunks without LRUDs",
          "[SurvexExport][SurvexLrud]") {
    const QString output = exportTrip({false, false, false, false});

    INFO("Exporter output:\n" << output.toStdString());
    CHECK(output.count(cwSurvexExporterUtils::passageDataHeader()) == 0);
}

TEST_CASE("Survex trip export writes one *data passage block per chunk with LRUDs",
          "[SurvexExport][SurvexLrud]") {
    const QString output = exportTrip({true, false});

    INFO("Exporter output:\n" << output.toStdString());
    CHECK(output.count(cwSurvexExporterUtils::passageDataHeader()) == 1);
    CHECK(firstPassageRow(output).toStdString() == "a1 1 2 3 4");
}
