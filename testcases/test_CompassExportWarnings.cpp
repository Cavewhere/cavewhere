/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Cavewhere includes
#include "cwCave.h"
#include "cwTrip.h"
#include "cwTeam.h"
#include "cwTeamMember.h"
#include "cwSurveyChunk.h"
#include "cwCompassExporterCaveTask.h"

//Qt includes
#include <QDir>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

namespace {

constexpr int kStationFieldWidth = 12;
constexpr int kTeamFieldWidth = 100;
constexpr int kTeamNameCount = 10;

const QStringList& teamNames()
{
    static const QStringList names = {
        QStringLiteral("Philip Schuchardt"),
        QStringLiteral("Bob Thrun"),
        QStringLiteral("Larry Fish"),
        QStringLiteral("Dave Bunnell"),
        QStringLiteral("Scott Fee"),
        QStringLiteral("Roger Brucker"),
        QStringLiteral("Patricia Crowther"),
        QStringLiteral("Red Watson"),
        QStringLiteral("John Wilcox"),
        QStringLiteral("Richard Zopf"),
    };
    return names;
}

//Builds a cave with one trip holding a single shot, fromStation -> toStation.
std::unique_ptr<cwCave> makeCave(const QString& fromStation, const QString& toStation)
{
    auto cave = std::make_unique<cwCave>();
    cave->setName(QStringLiteral("Short Cave"));

    cwTrip* trip = new cwTrip();
    trip->setName(QStringLiteral("Trip1"));
    cave->addTrip(trip);

    auto chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    chunk->appendNewShot();
    chunk->setData(cwSurveyChunk::StationNameRole, 0, fromStation);
    chunk->setData(cwSurveyChunk::StationNameRole, 1, toStation);
    chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, QStringLiteral("10.0"));
    chunk->setData(cwSurveyChunk::ShotCompassRole, 0, QStringLiteral("90"));
    chunk->setData(cwSurveyChunk::ShotClinoRole, 0, QStringLiteral("0"));

    return cave;
}

QStringList exportCave(const cwCave* cave, const QString& exportFile)
{
    auto exportToCompass = std::make_unique<cwCompassExportCaveTask>();
    exportToCompass->setData(cave->data());
    exportToCompass->setOutputFile(exportFile);
    exportToCompass->start();
    exportToCompass->waitToFinish();
    return exportToCompass->errors();
}

QString readFile(const QString& path)
{
    QFile file(path);
    REQUIRE(file.open(QFile::ReadOnly));
    return QString::fromUtf8(file.readAll());
}

} // namespace

TEST_CASE("Compass export reports no warnings for header fields that fit by padding", "[Compass][CompassExport]") {
    const QStringList names = teamNames().mid(0, kTeamNameCount);
    REQUIRE(names.size() == kTeamNameCount);
    const QString team = names.join(QStringLiteral(", "));
    REQUIRE(team.size() > kTeamFieldWidth);

    auto cave = makeCave(QStringLiteral("A1"), QStringLiteral("A2"));
    cwTrip* trip = cave->trips().first();
    for(const QString& name : names) {
        trip->team()->addTeamMember(cwTeamMember(name, QStringList()));
    }

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString exportFile = QDir(tempDir.path()).filePath(QStringLiteral("headerPadding.dat"));

    const QStringList errors = exportCave(cave.get(), exportFile);
    CAPTURE(errors.join(QStringLiteral(" | ")).toStdString());
    CHECK(errors.isEmpty());

    const QString contents = readFile(exportFile);
    CHECK(contents.contains(team));
}

TEST_CASE("Compass export warns once when it truncates a long station name", "[Compass][CompassExport]") {
    //The long station is only the shot's From; the LRUD line repeats the last
    //station (A2), so the long name is written exactly once.
    const QString longStation = QStringLiteral("ENTRANCELONGNAME1");
    REQUIRE(longStation.size() > kStationFieldWidth);
    const QString writtenStation = longStation.left(kStationFieldWidth);

    auto cave = makeCave(longStation, QStringLiteral("A2"));

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString exportFile = QDir(tempDir.path()).filePath(QStringLiteral("longStation.dat"));

    const QStringList errors = exportCave(cave.get(), exportFile);
    CAPTURE(errors.join(QStringLiteral(" | ")).toStdString());

    QStringList stationWarnings;
    for(const QString& error : errors) {
        if(error.contains(QStringLiteral("Truncating")) && error.contains(writtenStation)) {
            stationWarnings.append(error);
        }
    }

    REQUIRE(stationWarnings.size() == 1);
    const QString warning = stationWarnings.first();
    CAPTURE(warning.toStdString());
    CHECK(warning.contains(QStringLiteral("From")));
    CHECK(warning.contains(longStation));

    const QString contents = readFile(exportFile);
    CHECK(contents.contains(writtenStation));
    CHECK_FALSE(contents.contains(longStation));
}
