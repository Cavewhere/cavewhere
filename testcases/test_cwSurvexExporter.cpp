//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QFile>

//Our includes
#include "LoadProjectHelper.h"
#include "cwSurvexExporter.h"
#include "cwSurvexExporterRegion.h"
#include "cwCavingRegion.h"
#include "cwDistanceReading.h"
#include "cwCompassReading.h"
#include "cwClinoReading.h"
#include "cwLinePlotManager.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwStationPositionLookup.h"

//Catch includes
#include <catch2/matchers/catch_matchers_floating_point.hpp>

//Qt includes
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>
#include <QTextStream>

namespace {

//! The trip written to a string, with the writer's errors checked as empty
QString writeTripToString(const cwTripData& trip) {
    QByteArray outputData;
    QBuffer buffer(&outputData);
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    QStringList errors;
    {
        QTextStream stream(&buffer);
        cwSurvexExporter::writeTrip(stream, trip, errors);
    }
    buffer.close();
    INFO(errors.join('\n').toStdString());
    REQUIRE(errors.isEmpty());
    return QString::fromUtf8(outputData);
}

const QString kFrontSightDataLine = QStringLiteral("*data normal from to tape compass clino");
const QString kBackSightDataLine = QStringLiteral("*data normal from to tape backcompass backclino");
const QString kDataNormalPrefix = QStringLiteral("*data normal");

cwShot makeShot(const QString& distance,
                const QString& compass,
                const QString& clino,
                const QString& backCompass,
                const QString& backClino)
{
    cwShot shot;
    shot.setDistance(cwDistanceReading(distance));
    shot.setCompass(cwCompassReading(compass));
    shot.setClino(cwClinoReading(clino));
    shot.setBackCompass(cwCompassReading(backCompass));
    shot.setBackClino(cwClinoReading(backClino));
    return shot;
}

//! Trip alpha with one chunk a1, a2, ... joined by \a shots, under the given sight flags
cwTripData sightFlagTrip(bool hasFrontSights, bool hasBackSights, const QList<cwShot>& shots)
{
    cwTripData trip;
    trip.name = QStringLiteral("alpha");
    trip.calibrations.setFrontSights(hasFrontSights);
    trip.calibrations.setBackSights(hasBackSights);

    cwSurveyChunkData chunk;
    for(qsizetype i = 0; i <= shots.size(); i++) {
        chunk.stations.append(cwStation(QStringLiteral("a%1").arg(i + 1)));
    }
    chunk.shots = shots;
    trip.chunks.append(chunk);
    return trip;
}

QStringList trimmedLines(const QString& text)
{
    QStringList lines;
    const QStringList rawLines = text.split(QLatin1Char('\n'));
    for(const QString& line : rawLines) {
        lines.append(line.trimmed());
    }
    return lines;
}

QStringList tokens(const QString& line)
{
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    return line.split(whitespace, Qt::SkipEmptyParts);
}

//! Index of the leg line from \a from to \a to, or -1
qsizetype legLineIndex(const QStringList& lines, const QString& from, const QString& to)
{
    for(qsizetype i = 0; i < lines.size(); i++) {
        const QStringList lineTokens = tokens(lines.at(i));
        if(lineTokens.size() >= 2 && lineTokens.at(0) == from && lineTokens.at(1) == to) {
            return i;
        }
    }
    return -1;
}

//! The *data normal line in force at \a index: the last one written before it
QString activeDataLine(const QStringList& lines, qsizetype index)
{
    for(qsizetype i = index - 1; i >= 0; i--) {
        if(lines.at(i).startsWith(kDataNormalPrefix)) {
            return lines.at(i);
        }
    }
    return QString();
}

} // namespace

TEST_CASE("cwSurvexExporter should export a caving region correctly", "[cwSurvexExporter]") {
    // Load project and get the caving region
    auto project = fileToProject(testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
    auto cavingRegion = project->cavingRegion();

    //Concurrent test processes each need their own output file
    const QString exportedPath = QDir::temp().filePath(
        QStringLiteral("cwSurvexExporter-%1.svx").arg(QCoreApplication::applicationPid()));

    cwSurvexExporterRegion::Options options;
    options.outputCSPolicy = cwSurvexExporterRegion::OutputCSPolicy::Shareable;
    const auto result = cwSurvexExporterRegion::exportRegion(cavingRegion->data(), exportedPath, options);
    INFO(result.errorMessage().toStdString());
    REQUIRE_FALSE(result.hasError());

    INFO("Exported file:" << exportedPath.toStdString());

    QFile exportedFile(exportedPath);
    REQUIRE(exportedFile.open(QIODevice::ReadOnly));
    const QByteArray exportedContent = exportedFile.readAll();
    exportedFile.close();
    QFile::remove(exportedPath);

    // Load expected content from the known expected file
    QFile expectedFile(testcasesDatasetPath("test_cwSurvexExporter/PhakeCave3000_expected.svx"));
    REQUIRE(expectedFile.exists());
    REQUIRE(expectedFile.open(QIODevice::ReadOnly));
    const QByteArray expectedContent = expectedFile.readAll();
    expectedFile.close();

    auto normalizedLines = [](const QByteArray& bytes) {
        QString text = QString::fromUtf8(bytes);
        text.replace("\r\n", "\n");
        text.replace('\r', '\n');
        return text.split('\n', Qt::KeepEmptyParts);
    };

    const auto exportedLines = normalizedLines(exportedContent);
    const auto expectedLines = normalizedLines(expectedContent);
    REQUIRE(exportedLines.size() == expectedLines.size());

    for (int lineIndex = 0; lineIndex < exportedLines.size(); ++lineIndex) {
        INFO("line " << (lineIndex + 1) << ": exported '" << exportedLines.at(lineIndex).toStdString()
             << "' vs expected '" << expectedLines.at(lineIndex).toStdString() << "'");
        REQUIRE(exportedLines.at(lineIndex) == expectedLines.at(lineIndex));
    }
}

TEST_CASE("cwSurvexExporter writes UP/DOWN for vertical shots without azimuth", "[cwSurvexExporter]") {
    cwTripData trip;
    trip.name = QStringLiteral("VerticalTrip");
    trip.calibrations.setBackSights(false);

    cwSurveyChunkData chunk;
    cwStation stationA;
    cwStation stationB;
    stationA.setName(QStringLiteral("a1"));
    stationB.setName(QStringLiteral("a2"));
    chunk.stations.append(stationA);
    chunk.stations.append(stationB);

    cwShot shotUp;
    shotUp.setDistance(cwDistanceReading(QStringLiteral("10")));
    shotUp.setCompass(cwCompassReading(QString()));
    shotUp.setClino(cwClinoReading(QStringLiteral("90")));
    chunk.shots.append(shotUp);

    cwShot shotDown;
    shotDown.setDistance(cwDistanceReading(QStringLiteral("10")));
    shotDown.setCompass(cwCompassReading(QString()));
    shotDown.setClino(cwClinoReading(QStringLiteral("-90")));
    chunk.stations.append(stationA);
    chunk.shots.append(shotDown);

    trip.chunks.append(chunk);

    const QString output = writeTripToString(trip);

    QStringList dataLines;
    const QStringList lines = output.split('\n');
    for(const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if(trimmed.isEmpty() || trimmed.startsWith('*') || trimmed.startsWith(';')) {
            continue;
        }
        if(trimmed.contains("a1") && trimmed.contains("a2")) {
            dataLines.append(trimmed);
        }
    }

    REQUIRE(dataLines.size() >= 2);
    CHECK(dataLines.at(0).contains("UP"));
    CHECK(dataLines.at(1).contains("DOWN"));
}

TEST_CASE("cwSurvexExporter writes UP/DOWN for vertical shots with azimuth", "[cwSurvexExporter]") {
    cwTripData trip;
    trip.name = QStringLiteral("VerticalTripWithAzimuth");
    trip.calibrations.setBackSights(false);

    cwSurveyChunkData chunk;
    cwStation stationA;
    cwStation stationB;
    stationA.setName(QStringLiteral("a1"));
    stationB.setName(QStringLiteral("a2"));
    chunk.stations.append(stationA);
    chunk.stations.append(stationB);

    cwShot shotUp;
    shotUp.setDistance(cwDistanceReading(QStringLiteral("10")));
    shotUp.setCompass(cwCompassReading(QStringLiteral("123")));
    shotUp.setClino(cwClinoReading(QStringLiteral("90")));
    chunk.shots.append(shotUp);

    cwShot shotDown;
    shotDown.setDistance(cwDistanceReading(QStringLiteral("10")));
    shotDown.setCompass(cwCompassReading(QStringLiteral("222")));
    shotDown.setClino(cwClinoReading(QStringLiteral("-90")));
    chunk.stations.append(stationA);
    chunk.shots.append(shotDown);

    trip.chunks.append(chunk);

    const QString output = writeTripToString(trip);

    QStringList dataLines;
    const QStringList lines = output.split('\n');
    for(const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if(trimmed.isEmpty() || trimmed.startsWith('*') || trimmed.startsWith(';')) {
            continue;
        }
        if(trimmed.contains("a1") && trimmed.contains("a2")) {
            dataLines.append(trimmed);
        }
    }

    REQUIRE(dataLines.size() >= 2);
    CHECK(dataLines.at(0).contains("123"));
    CHECK(dataLines.at(0).contains("UP"));
    CHECK(!dataLines.at(0).contains(" 90"));
    CHECK(dataLines.at(1).contains("222"));
    CHECK(dataLines.at(1).contains("DOWN"));
    CHECK(!dataLines.at(1).contains(" -90"));
}

TEST_CASE("cwSurvexExporter equates LRUD-only carrier shots", "[cwSurvexExporter]") {
    // Compass records passage dimensions on a synthetic zero-length shot with
    // no compass/clino (e.g. "a1lrud a1 0 - - - -"). Survex can't parse that as
    // a leg, so it must be exported as "*equate a1lrud a1".
    cwTripData trip;
    trip.name = QStringLiteral("LrudCarrierTrip");
    trip.calibrations.setBackSights(false);

    cwSurveyChunkData chunk;
    cwStation carrier;
    cwStation real;
    carrier.setName(QStringLiteral("a1lrud"));
    carrier.setLeft(cwDistanceReading(QStringLiteral("12")));
    carrier.setRight(cwDistanceReading(QStringLiteral("1")));
    carrier.setUp(cwDistanceReading(QStringLiteral("10")));
    carrier.setDown(cwDistanceReading(QStringLiteral("2")));
    real.setName(QStringLiteral("a1"));
    chunk.stations.append(carrier);
    chunk.stations.append(real);

    cwShot lrudOnly;
    lrudOnly.setDistance(cwDistanceReading(QStringLiteral("0")));
    chunk.shots.append(lrudOnly);

    trip.chunks.append(chunk);

    const QString output = writeTripToString(trip);
    CHECK(output.contains(QStringLiteral("*equate a1lrud a1")));

    // The carrier must not appear as a normal data leg. It still legitimately
    // appears in the *data passage block (that's why it's equated), so only
    // inspect lines inside the *data normal block.
    const QStringList lines = output.split('\n');
    bool inNormalBlock = false;
    for(const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if(trimmed.startsWith(QStringLiteral("*data normal"))) {
            inNormalBlock = true;
            continue;
        }
        if(trimmed.startsWith(QStringLiteral("*data passage"))) {
            inNormalBlock = false;
            continue;
        }
        if(!inNormalBlock || trimmed.startsWith('*') || trimmed.startsWith(';')) {
            continue;
        }
        CHECK_FALSE(trimmed.startsWith(QStringLiteral("a1lrud")));
    }
}

TEST_CASE("cwSurvexExporter rewrites scientific-notation distances", "[cwSurvexExporter]") {
    // Older imports could store a distance/LRUD string in scientific notation
    // (e.g. "1.1e+02"). Survex's parser rejects it, so the exporter must emit a
    // plain decimal.
    cwTripData trip;
    trip.name = QStringLiteral("ScientificTrip");
    trip.calibrations.setBackSights(false);

    cwSurveyChunkData chunk;
    cwStation stationA;
    cwStation stationB;
    stationA.setName(QStringLiteral("a1"));
    stationA.setUp(cwDistanceReading(QStringLiteral("1.1e+02")));
    stationB.setName(QStringLiteral("a2"));
    chunk.stations.append(stationA);
    chunk.stations.append(stationB);

    cwShot shot;
    shot.setDistance(cwDistanceReading(QStringLiteral("1.1e+02")));
    shot.setCompass(cwCompassReading(QStringLiteral("123")));
    shot.setClino(cwClinoReading(QStringLiteral("2")));
    chunk.shots.append(shot);

    trip.chunks.append(chunk);

    const QString output = writeTripToString(trip);
    CHECK_FALSE(output.contains(QStringLiteral("e+02")));

    // The distance must be rewritten as a plain decimal on the actual data leg,
    // not merely absent from the whole file.
    QString dataLine;
    const QStringList lines = output.split('\n');
    for(const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if(trimmed.startsWith('*') || trimmed.startsWith(';')) {
            continue;
        }
        if(trimmed.startsWith(QStringLiteral("a1")) && trimmed.contains(QStringLiteral("a2"))) {
            dataLine = trimmed;
            break;
        }
    }
    REQUIRE_FALSE(dataLine.isEmpty());
    CHECK(dataLine.contains(QStringLiteral("110.00")));
    CHECK_FALSE(dataLine.contains(QLatin1Char('e')));
}

TEST_CASE("cwSurvexExporter writes a back-only trip's front-only leg under front-sight columns", "[cwSurvexExporter]") {
    const cwTripData trip = sightFlagTrip(false, true, {
        makeShot(QStringLiteral("10.0"), QStringLiteral("90"), QStringLiteral("0"), QString(), QString()),
        makeShot(QStringLiteral("5.0"), QString(), QStringLiteral("DOWN"), QString(), QStringLiteral("UP"))
    });

    const QStringList lines = trimmedLines(writeTripToString(trip));
    INFO(lines.join(QLatin1Char('\n')).toStdString());

    const qsizetype a1a2 = legLineIndex(lines, QStringLiteral("a1"), QStringLiteral("a2"));
    const qsizetype a2a3 = legLineIndex(lines, QStringLiteral("a2"), QStringLiteral("a3"));
    REQUIRE(a1a2 >= 0);
    REQUIRE(a2a3 > a1a2);

    CHECK(activeDataLine(lines, a1a2).toStdString() == kFrontSightDataLine.toStdString());
    CHECK(tokens(lines.at(a1a2)) == QStringList({QStringLiteral("a1"), QStringLiteral("a2"),
                                                 QStringLiteral("10.0"), QStringLiteral("90"), QStringLiteral("0")}));

    CHECK(activeDataLine(lines, a2a3).toStdString() == kBackSightDataLine.toStdString());
    CHECK(tokens(lines.at(a2a3)) == QStringList({QStringLiteral("a2"), QStringLiteral("a3"),
                                                 QStringLiteral("5.0"), QStringLiteral("-"), QStringLiteral("UP")}));
}

TEST_CASE("cwSurvexExporter writes a front-only trip's back-only leg under backsight columns", "[cwSurvexExporter]") {
    const cwTripData trip = sightFlagTrip(true, false, {
        makeShot(QStringLiteral("10.0"), QString(), QString(), QStringLiteral("270"), QStringLiteral("0")),
        makeShot(QStringLiteral("5.0"), QString(), QStringLiteral("DOWN"), QString(), QStringLiteral("UP"))
    });

    const QStringList lines = trimmedLines(writeTripToString(trip));
    INFO(lines.join(QLatin1Char('\n')).toStdString());

    const qsizetype a1a2 = legLineIndex(lines, QStringLiteral("a1"), QStringLiteral("a2"));
    const qsizetype a2a3 = legLineIndex(lines, QStringLiteral("a2"), QStringLiteral("a3"));
    REQUIRE(a1a2 >= 0);
    REQUIRE(a2a3 > a1a2);

    CHECK(activeDataLine(lines, a1a2).toStdString() == kBackSightDataLine.toStdString());
    CHECK(tokens(lines.at(a1a2)) == QStringList({QStringLiteral("a1"), QStringLiteral("a2"),
                                                 QStringLiteral("10.0"), QStringLiteral("270"), QStringLiteral("0")}));

    CHECK(activeDataLine(lines, a2a3).toStdString() == kFrontSightDataLine.toStdString());
    CHECK(tokens(lines.at(a2a3)) == QStringList({QStringLiteral("a2"), QStringLiteral("a3"),
                                                 QStringLiteral("5.0"), QStringLiteral("-"), QStringLiteral("DOWN")}));
}

TEST_CASE("cwSurvexExporter switches the column set once per run of stale-flag legs", "[cwSurvexExporter]") {
    const cwTripData trip = sightFlagTrip(false, true, {
        makeShot(QStringLiteral("10.0"), QStringLiteral("90"), QStringLiteral("0"), QString(), QString()),
        makeShot(QStringLiteral("10.0"), QStringLiteral("180"), QStringLiteral("0"), QString(), QString()),
        makeShot(QStringLiteral("5.0"), QString(), QStringLiteral("DOWN"), QString(), QStringLiteral("UP"))
    });

    const QStringList lines = trimmedLines(writeTripToString(trip));
    INFO(lines.join(QLatin1Char('\n')).toStdString());

    const qsizetype a1a2 = legLineIndex(lines, QStringLiteral("a1"), QStringLiteral("a2"));
    const qsizetype a2a3 = legLineIndex(lines, QStringLiteral("a2"), QStringLiteral("a3"));
    const qsizetype a3a4 = legLineIndex(lines, QStringLiteral("a3"), QStringLiteral("a4"));
    REQUIRE(a1a2 >= 0);
    REQUIRE(a2a3 > a1a2);
    REQUIRE(a3a4 > a2a3);

    CHECK(activeDataLine(lines, a1a2).toStdString() == kFrontSightDataLine.toStdString());
    CHECK(activeDataLine(lines, a2a3).toStdString() == kFrontSightDataLine.toStdString());
    CHECK(activeDataLine(lines, a3a4).toStdString() == kBackSightDataLine.toStdString());
    CHECK(lines.count(kFrontSightDataLine) == 1);
}

TEST_CASE("cwSurvexExporter restores the trip's column set when a chunk ends on a stale-flag leg", "[cwSurvexExporter]") {
    cwTripData trip = sightFlagTrip(false, true, {
        makeShot(QStringLiteral("10.0"), QStringLiteral("90"), QStringLiteral("0"), QString(), QString())
    });

    cwSurveyChunkData secondChunk;
    secondChunk.stations.append(cwStation(QStringLiteral("a2")));
    secondChunk.stations.append(cwStation(QStringLiteral("a3")));
    secondChunk.shots.append(makeShot(QStringLiteral("5.0"), QString(), QStringLiteral("DOWN"), QString(), QStringLiteral("UP")));
    trip.chunks.append(secondChunk);

    const QStringList lines = trimmedLines(writeTripToString(trip));
    INFO(lines.join(QLatin1Char('\n')).toStdString());

    const qsizetype a1a2 = legLineIndex(lines, QStringLiteral("a1"), QStringLiteral("a2"));
    const qsizetype a2a3 = legLineIndex(lines, QStringLiteral("a2"), QStringLiteral("a3"));
    REQUIRE(a1a2 >= 0);
    REQUIRE(a2a3 > a1a2);

    CHECK(activeDataLine(lines, a1a2).toStdString() == kFrontSightDataLine.toStdString());
    CHECK(activeDataLine(lines, a2a3).toStdString() == kBackSightDataLine.toStdString());
    CHECK(tokens(lines.at(a2a3)) == QStringList({QStringLiteral("a2"), QStringLiteral("a3"),
                                                 QStringLiteral("5.0"), QStringLiteral("-"), QStringLiteral("UP")}));
}

TEST_CASE("cwSurvexExporter stale sight flags still solve through cavern", "[cwSurvexExporter]") {
    cwTripData tripData;

    SECTION("Back-only trip with front-only legs") {
        tripData = sightFlagTrip(false, true, {
            makeShot(QStringLiteral("10.0"), QStringLiteral("90"), QStringLiteral("0"), QString(), QString()),
            makeShot(QStringLiteral("5.0"), QString(), QStringLiteral("DOWN"), QString(), QStringLiteral("UP"))
        });
    }

    SECTION("Front-only trip with back-only legs") {
        tripData = sightFlagTrip(true, false, {
            makeShot(QStringLiteral("10.0"), QString(), QString(), QStringLiteral("270"), QStringLiteral("0")),
            makeShot(QStringLiteral("5.0"), QString(), QStringLiteral("DOWN"), QString(), QStringLiteral("UP"))
        });
    }

    cwCavingRegion region;
    auto cave = new cwCave();
    cave->setName(QStringLiteral("Phake Cave"));
    region.addCave(cave);

    auto trip = new cwTrip();
    cave->addTrip(trip);
    trip->setData(tripData);

    auto plotManager = std::make_unique<cwLinePlotManager>();
    plotManager->setRegion(&region);
    plotManager->waitToFinish();

    INFO(plotManager->solveErrorMessage().toStdString());
    CHECK_FALSE(plotManager->hasSolveError());

    const cwStationPositionLookup positions = cave->scopedPositions();
    REQUIRE(positions.hasPosition(QStringLiteral("a3")));
    REQUIRE(positions.hasPosition(QStringLiteral("a1")));

    //Both variants run 10 m east and then 5 m straight down
    constexpr double kTolerance = 0.01;
    const QVector3D a3FromA1 = positions.position(QStringLiteral("a3")) - positions.position(QStringLiteral("a1"));
    CHECK_THAT(a3FromA1.x(), Catch::Matchers::WithinAbs(10.0, kTolerance));
    CHECK_THAT(a3FromA1.y(), Catch::Matchers::WithinAbs(0.0, kTolerance));
    CHECK_THAT(a3FromA1.z(), Catch::Matchers::WithinAbs(-5.0, kTolerance));
}
