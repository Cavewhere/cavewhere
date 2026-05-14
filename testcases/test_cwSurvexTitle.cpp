/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Cavewhere includes
#include "cwSurvexImporter.h"
#include "cwSurvexExporterTripTask.h"
#include "cwTreeImportDataNode.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwTripCalibration.h"

//Qt includes
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QString>

namespace {

QString writeSvxFile(QTemporaryDir& dir, const QString& body) {
    REQUIRE(dir.isValid());
    QString path = dir.filePath(QStringLiteral("title.svx"));
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&file);
    stream << body;
    file.close();
    return path;
}

cwTrip* importSingleTrip(cwSurvexImporter& importer, const QString& path) {
    importer.setInputFiles(QStringList() << path);
    importer.start();
    importer.waitToFinish();

    REQUIRE(importer.hasParseErrors() == false);
    REQUIRE(importer.data()->nodes().size() == 1);

    importer.data()->nodes().first()->setImportType(cwTreeImportDataNode::Trip);

    QList<cwCave*> caves = importer.data()->caves();
    REQUIRE(caves.size() == 1);
    REQUIRE(caves.first()->trips().size() == 1);
    return caves.first()->trips().first();
}

} // namespace

TEST_CASE("Survex *title becomes the trip name", "[SurvexImport][SurvexTitle]") {
    const QString body =
            "*begin shortName\n"
            "*title \"Friendly Trip Name With Spaces\"\n"
            "*data normal from to tape compass clino\n"
            "1 2 5.0 10 0\n"
            "*end shortName\n";

    QTemporaryDir dir;
    QString path = writeSvxFile(dir, body);

    cwSurvexImporter importer;
    cwTrip* trip = importSingleTrip(importer, path);

    CHECK(trip->name().toStdString() == "Friendly Trip Name With Spaces");
}

TEST_CASE("Survex *title accepts an unquoted value", "[SurvexImport][SurvexTitle]") {
    const QString body =
            "*begin foo\n"
            "*title BareTitle\n"
            "*data normal from to tape compass clino\n"
            "1 2 5.0 10 0\n"
            "*end foo\n";

    QTemporaryDir dir;
    QString path = writeSvxFile(dir, body);

    cwSurvexImporter importer;
    cwTrip* trip = importSingleTrip(importer, path);

    CHECK(trip->name().toStdString() == "BareTitle");
}

TEST_CASE("Without *title, trip name falls back to the *begin block name", "[SurvexImport][SurvexTitle]") {
    const QString body =
            "*begin myBlock\n"
            "*data normal from to tape compass clino\n"
            "1 2 5.0 10 0\n"
            "*end myBlock\n";

    QTemporaryDir dir;
    QString path = writeSvxFile(dir, body);

    cwSurvexImporter importer;
    cwTrip* trip = importSingleTrip(importer, path);

    CHECK(trip->name().toStdString() == "myBlock");
}

TEST_CASE("Survex exporter emits a *title line carrying the trip name", "[SurvexExport][SurvexTitle]") {
    cwTrip trip;
    trip.setName(QStringLiteral("Big Room Survey"));

    QString output;
    QTextStream stream(&output);

    cwSurvexExporterTripTask exporter;
    exporter.writeTrip(stream, &trip);

    INFO("Exporter output:\n" << output.toStdString());
    CHECK(output.contains(QStringLiteral("*title \"Big Room Survey\"")));
}

TEST_CASE("Trip names with spaces survive a Survex export -> import round-trip", "[SurvexExport][SurvexImport][SurvexTitle]") {
    cwTrip trip;
    trip.setName(QStringLiteral("My Survey With Spaces"));
    //Front-sights only keeps the exported `*data normal` line in its simplest
    //5-column form (from to tape compass clino), avoiding back-sight gymnastics.
    trip.calibrations()->setFrontSights(true);
    trip.calibrations()->setBackSights(false);
    cwSurveyChunk* chunk = new cwSurveyChunk(&trip);
    cwShot shot;
    shot.setDistance(QStringLiteral("5.0"));
    shot.setCompass(QStringLiteral("10"));
    shot.setClino(QStringLiteral("0"));
    chunk->appendShot(cwStation(QStringLiteral("1")), cwStation(QStringLiteral("2")), shot);
    trip.addChunk(chunk);

    QString svxBody;
    {
        QTextStream stream(&svxBody);
        cwSurvexExporterTripTask exporter;
        exporter.writeTrip(stream, &trip);
    }

    QTemporaryDir dir;
    QString path = writeSvxFile(dir, svxBody);

    cwSurvexImporter importer;
    cwTrip* importedTrip = importSingleTrip(importer, path);

    CHECK(importedTrip->name().toStdString() == "My Survey With Spaces");
}
