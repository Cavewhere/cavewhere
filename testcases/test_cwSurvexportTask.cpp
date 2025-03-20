//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwSurvexportTask.h"
#include "cwGlobals.h"
#include "LoadProjectHelper.h"
#include "cwCavernTask.h"

//Qt includes
#include <QFileInfo>

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
