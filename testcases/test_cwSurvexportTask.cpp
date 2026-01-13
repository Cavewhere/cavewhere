//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwSurvexportTask.h"
#include "cwGlobals.h"
#include "LoadProjectHelper.h"
#include "cwCavernTask.h"
#include "cwSurvexExporterTripTask.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"

//Qt includes
#include <QBuffer>
#include <QFileInfo>
#include <QTextStream>

TEST_CASE("cwSurvexportTask should initilize correctly", "[cwSurvexportTask]") {
    cwSurvexportTask task;
    CHECK(task.outputFilename().toStdString() == "");
}

TEST_CASE("cwSurvexportTask should produce a CSV file from a .3d file", "[cwSurvexportTask]") {
    //Run cavern first to get 3d file

    QStringList cavernNames;
    cavernNames.append("cavern");
    cavernNames.append("cavern.exe");
    cavernNames.append("survex/cavern");
    cavernNames.append("survex/cavern.exe");

    QString cavernPath = cwGlobals::findExecutable(cavernNames, {cwGlobals::survexPath()});

    //If this fails, cavern couldn't be found
    REQUIRE(cavernPath.isEmpty() == false);

    QString cavernDataFile = copyToTempFolder("://datasets/test_cwSurvexport/data.svx");
    QDir tempDir = QFileInfo(cavernDataFile).absoluteDir();

    REQUIRE(QFile::exists(cavernDataFile));

    cwCavernTask cavern;
    cavern.setSurvexFile(cavernDataFile);
    cavern.start();
    cavern.waitToFinish();

    REQUIRE(QFileInfo(cavern.output3dFileName()).exists() == true);

    cwSurvexportTask task;
    task.setSurvex3DFile(cavern.output3dFileName());
    task.start();
    task.waitToFinish();

    //If this fails the task didn't create csv
    REQUIRE(task.outputFilename().isEmpty() == false);

    //Compare the test file with the generated file
    QFile generatedFile(task.outputFilename());
    QFile testFile("://datasets/test_cwSurvexport/cwSurvexport_data.3d.csv");
    generatedFile.open(QFile::ReadOnly);
    testFile.open(QFile::ReadOnly);

    INFO("GeneratedFile errorStr:" << generatedFile.errorString().toStdString());
    CHECK(generatedFile.error() == QFile::NoError);

    INFO("TestFile errorStr:" << testFile.errorString().toStdString());
    CHECK(testFile.error() == QFile::NoError);

    int line = 1;
    while(!generatedFile.atEnd() && !testFile.atEnd()) {
        QString generate = generatedFile.readLine();
        QString test = testFile.readLine();
        INFO("Line:" << line);
        CHECK(generate.trimmed().toStdString() == test.trimmed().toStdString());
        line++;
    }

    CHECK(generatedFile.atEnd() == true);
    CHECK(testFile.atEnd() == true);
}

TEST_CASE("cwSurvexExporterTripTask writes UP/DOWN for vertical shots without azimuth", "[cwSurvexExporterTripTask]") {
    cwTrip trip;
    trip.setName(QStringLiteral("TestTrip"));
    trip.calibrations()->setBackSights(false);

    auto chunk = new cwSurveyChunk();
    trip.addChunk(chunk);

    chunk->appendNewShot();
    chunk->setData(cwSurveyChunk::StationNameRole, 0, "a1");
    chunk->setData(cwSurveyChunk::StationNameRole, 1, "a2");
    chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, "10");
    chunk->setData(cwSurveyChunk::ShotCompassRole, 0, "");
    chunk->setData(cwSurveyChunk::ShotClinoRole, 0, "90");

    cwSurvexExporterTripTask exporter;
    QByteArray outputData;
    QBuffer buffer(&outputData);
    REQUIRE(buffer.open(QIODevice::WriteOnly));

    QTextStream stream(&buffer);
    exporter.writeTrip(stream, &trip);
    buffer.close();

    const QString output = QString::fromUtf8(outputData);
    QString dataLine;
    const QStringList lines = output.split('\n');
    for(const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if(trimmed.isEmpty() || trimmed.startsWith('*') || trimmed.startsWith(';')) {
            continue;
        }
        if(trimmed.contains("a1", Qt::CaseInsensitive) && trimmed.contains("a2", Qt::CaseInsensitive)) {
            dataLine = trimmed;
            break;
        }
    }

    if(dataLine.isEmpty()) {
        for(const QString& line : lines) {
            const QString trimmed = line.trimmed();
            if(trimmed.isEmpty() || trimmed.startsWith('*') || trimmed.startsWith(';')) {
                continue;
            }
            dataLine = trimmed;
            break;
        }
    }

    REQUIRE(!dataLine.isEmpty());
    CHECK(dataLine.contains("UP"));
    CHECK(!dataLine.contains(" 90"));
}

TEST_CASE("cwSurvexExporterTripTask writes UP/DOWN for vertical shots with azimuth", "[cwSurvexExporterTripTask]") {
    cwTrip trip;
    trip.setName(QStringLiteral("TestTrip"));
    trip.calibrations()->setBackSights(false);

    auto chunk = new cwSurveyChunk();
    trip.addChunk(chunk);

    chunk->appendNewShot();
    chunk->setData(cwSurveyChunk::StationNameRole, 0, "a1");
    chunk->setData(cwSurveyChunk::StationNameRole, 1, "a2");
    chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, "10");
    chunk->setData(cwSurveyChunk::ShotCompassRole, 0, "123");
    chunk->setData(cwSurveyChunk::ShotClinoRole, 0, "90");

    cwSurvexExporterTripTask exporter;
    QByteArray outputData;
    QBuffer buffer(&outputData);
    REQUIRE(buffer.open(QIODevice::WriteOnly));

    QTextStream stream(&buffer);
    exporter.writeTrip(stream, &trip);
    buffer.close();

    const QString output = QString::fromUtf8(outputData);
    QString dataLine;
    const QStringList lines = output.split('\n');
    for(const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if(trimmed.isEmpty() || trimmed.startsWith('*') || trimmed.startsWith(';')) {
            continue;
        }
        if(trimmed.contains("a1", Qt::CaseInsensitive) && trimmed.contains("a2", Qt::CaseInsensitive)) {
            dataLine = trimmed;
            break;
        }
    }

    REQUIRE(!dataLine.isEmpty());
    CHECK(dataLine.contains("123"));
    CHECK(dataLine.contains("UP"));
    CHECK(!dataLine.contains(" 90"));
}
