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
    auto project = fileToProject("://datasets/test_cwProject/Phake Cave 3000.cw");
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
        QFile expectedFile("://datasets/test_cwSurvexExporterRule/PhakeCave3000_expected.svx");
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
