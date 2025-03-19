//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QFile>

//Our includes
#include "LoadProjectHelper.h"
#include "cwSurvexExporterRule.h"
#include "cwSurveyDataArtifact.h"
#include "cwTemporaryFileArtifact.h"
#include "cwAsyncFuture.h"

TEST_CASE("cwSurvexExportRule should export a caving region correctly", "[cwSurvexExportRule]") {
    // Load project and get the caving region
    auto project = fileToProject("://datasets/test_cwProject/Phake Cave 3000.cw");
    auto cavingRegion = project->cavingRegion();

    // Create a survey data artifact and set the region
    cwSurveyDataArtifact surveyData;
    surveyData.setRegion(cavingRegion);

    // Use cwTemporaryFileArtifact to generate a temporary filename
    cwTemporaryFileArtifact tempFileArtifact;
    tempFileArtifact.setSuffix("cwSurvexExportRule.svx");

    // Create the exporter and set the necessary inputs
    cwSurvexExporterRule exporter;
    exporter.setSurveyData(&surveyData);
    exporter.setSurvexFileName(&tempFileArtifact);

    // Wait for the asynchronous export to complete (2000ms timeout)
    auto future = exporter.survexFileArtifact()->filename();
    CHECK(cwAsyncFuture::waitForFinished(future, 2000));

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

        // Compare the entire contents
        REQUIRE(exportedContent == expectedContent);
    }
}
