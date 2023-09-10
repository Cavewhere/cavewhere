//Catch includes
#include <catch.hpp>

//Our includes
#include "cwSurvexportTask.h"
#include "cwGlobals.h"

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

    QString cavernPath = cwGlobals::findExecutable(cavernNames, {cwGlobals::survexPath()});

    //If this fails, cavern couldn't be found
    REQUIRE(cavernPath.isEmpty() == false);

    QString cavernDataFile = QStringLiteral("cwSurvexport_data.svx");
    QString data3DFile = QStringLiteral("cwSurvexport_data.3d");
    QString csvFile = QStringLiteral("cwSurvexport_data.3d.csv");

    QFile::remove(cavernDataFile);
    QFile::remove(data3DFile);
    QFile::remove(csvFile);

    QFile::copy("://datasets/test_cwSurvexport/data.svx", cavernDataFile);

    QStringList arguments;
    arguments.append(cavernDataFile);

    QProcess cavernProcess;
    cavernProcess.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    cavernProcess.start(cavernPath, arguments);
    cavernProcess.waitForFinished();

    REQUIRE(QFileInfo(data3DFile).exists() == true);

    cwSurvexportTask task;
    task.setSurvex3DFile(data3DFile);
    task.start();
    task.waitToFinish();

    //If this fails the task didn't create csv
    REQUIRE(task.outputFilename().isEmpty() == false);

    //Compare the test file with the generated file
    QFile generatedFile(csvFile);
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
        CHECK(generate.toStdString() == test.toStdString());
        line++;
    }

    CHECK(generatedFile.atEnd() == true);
    CHECK(testFile.atEnd() == true);
}
