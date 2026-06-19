//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QFile>

//Our includes
#include "LoadProjectHelper.h"
#include "cwSurvexExporterRule.h"
#include "cwSurveyDataArtifact.h"
#include "cwTemporaryFileNameArtifact.h"
#include "asyncfuture.h"
#include "cwDistanceReading.h"
#include "cwCompassReading.h"
#include "cwClinoReading.h"

//Qt includes
#include <QBuffer>
#include <QTextStream>

TEST_CASE("cwSurvexExportRule should export a caving region correctly", "[cwSurvexExportRule]") {
    // Load project and get the caving region
    auto project = fileToProject(testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
    auto cavingRegion = project->cavingRegion();

    // Create a survey data artifact and set the region
    cwSurveyDataArtifact surveyData;
    surveyData.setRegion(cavingRegion);

    // Use cwTemporaryFileNameArtifact to generate a temporary filename
    cwTemporaryFileNameArtifact tempFileArtifact;
    tempFileArtifact.setSuffix("cwSurvexExportRule.svx");

    // Create the exporter and set the necessary inputs
    cwSurvexExporterRule exporter;
    exporter.setSurveyData(&surveyData);
    exporter.setSurvexFileName(&tempFileArtifact);

    // Wait for the asynchronous export to complete (2000ms timeout)
    auto future = exporter.survexFileArtifact()->filename();
    CHECK(AsyncFuture::waitForFinished(future, 2000));

    INFO("TempFileArtifact:" << tempFileArtifact.filename().toStdString());

    // Verify the exported file exists and has data
    QFile exportedFile(tempFileArtifact.filename());
    bool fileExists = exportedFile.exists();
    REQUIRE(fileExists == true);

    if (fileExists) {
        bool openOk = exportedFile.open(QIODevice::ReadOnly);
        REQUIRE(openOk == true);
        QByteArray exportedContent = exportedFile.readAll();
        exportedFile.close();

        // Load expected content from the known expected file
        QFile expectedFile(testcasesDatasetPath("test_cwSurvexExporterRule/PhakeCave3000_expected.svx"));
        bool expectedExists = expectedFile.exists();
        REQUIRE(expectedExists == true);
        bool expectedOpenOk = expectedFile.open(QIODevice::ReadOnly);
        REQUIRE(expectedOpenOk == true);
        QByteArray expectedContent = expectedFile.readAll();
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
}

TEST_CASE("cwSurvexExportRule writes UP/DOWN for vertical shots without azimuth", "[cwSurvexExportRule]") {
    cwSurveyDataArtifact::Trip trip;
    trip.name = QStringLiteral("VerticalTrip");
    trip.calibration.setBackSights(false);

    cwSurveyDataArtifact::SurveyChunk chunk;
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

    QByteArray outputData;
    QBuffer buffer(&outputData);
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    QTextStream stream(&buffer);
    auto result = cwSurvexExporterRule::writeTrip(stream, trip);
    buffer.close();

    REQUIRE(!result.hasError());

    const QString output = QString::fromUtf8(outputData);
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

TEST_CASE("cwSurvexExportRule writes UP/DOWN for vertical shots with azimuth", "[cwSurvexExportRule]") {
    cwSurveyDataArtifact::Trip trip;
    trip.name = QStringLiteral("VerticalTripWithAzimuth");
    trip.calibration.setBackSights(false);

    cwSurveyDataArtifact::SurveyChunk chunk;
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

    QByteArray outputData;
    QBuffer buffer(&outputData);
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    QTextStream stream(&buffer);
    auto result = cwSurvexExporterRule::writeTrip(stream, trip);
    buffer.close();

    REQUIRE(!result.hasError());

    const QString output = QString::fromUtf8(outputData);
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

TEST_CASE("cwSurvexExportRule equates LRUD-only carrier shots", "[cwSurvexExportRule]") {
    // Compass records passage dimensions on a synthetic zero-length shot with
    // no compass/clino (e.g. "a1lrud a1 0 - - - -"). Survex can't parse that as
    // a leg, so it must be exported as "*equate a1lrud a1".
    cwSurveyDataArtifact::Trip trip;
    trip.name = QStringLiteral("LrudCarrierTrip");
    trip.calibration.setBackSights(false);

    cwSurveyDataArtifact::SurveyChunk chunk;
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

    QByteArray outputData;
    QBuffer buffer(&outputData);
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    QTextStream stream(&buffer);
    auto result = cwSurvexExporterRule::writeTrip(stream, trip);
    buffer.close();

    REQUIRE(!result.hasError());

    const QString output = QString::fromUtf8(outputData);
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

TEST_CASE("cwSurvexExportRule rewrites scientific-notation distances", "[cwSurvexExportRule]") {
    // Older imports could store a distance/LRUD string in scientific notation
    // (e.g. "1.1e+02"). Survex's parser rejects it, so the exporter must emit a
    // plain decimal.
    cwSurveyDataArtifact::Trip trip;
    trip.name = QStringLiteral("ScientificTrip");
    trip.calibration.setBackSights(false);

    cwSurveyDataArtifact::SurveyChunk chunk;
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

    QByteArray outputData;
    QBuffer buffer(&outputData);
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    QTextStream stream(&buffer);
    auto result = cwSurvexExporterRule::writeTrip(stream, trip);
    buffer.close();

    REQUIRE(!result.hasError());

    const QString output = QString::fromUtf8(outputData);
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
