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

//Qt includes
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
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
