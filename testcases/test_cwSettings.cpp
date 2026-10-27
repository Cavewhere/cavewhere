//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwSettings.h"

//Qt includes
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

TEST_CASE("cwSettings should initilize correctly", "[cwSettings]") {
    cwSettings::initialize();
    auto settings1 = cwSettings::instance();
    cwSettings::initialize();
    auto settings2 = cwSettings::instance();

    CHECK(settings1 == settings2);
    REQUIRE(settings1);

    // CHECK(settings1->renderingSettings());
    CHECK(settings1->jobSettings());
}

TEST_CASE("Test harness keeps QSettings in a per-process temporary directory", "[cwSettings][TestHarness]") {
    const QString tempPath = QDir::tempPath();

    {
        QSettings settings;
        INFO("fileName: " << settings.fileName().toStdString());
        CHECK(settings.format() == QSettings::IniFormat);
        CHECK(settings.fileName().startsWith(tempPath));
        CHECK(settings.fileName().contains(QStringLiteral("/cavewhere-test-settings-")));

        settings.setValue(QStringLiteral("testHarness/probe"), true);
        settings.sync();
        CHECK(QFileInfo::exists(settings.fileName()));
        settings.remove(QStringLiteral("testHarness"));
    }

    SECTION("A swapped application name stays in the temporary directory")
    {
        const QString originalName = QCoreApplication::applicationName();
        const QString swappedName = originalName + QStringLiteral("-harnessSwap");
        QCoreApplication::setApplicationName(swappedName);

        {
            QSettings settings;
            INFO("fileName: " << settings.fileName().toStdString());
            CHECK(settings.format() == QSettings::IniFormat);
            CHECK(settings.fileName().startsWith(tempPath));
            CHECK(settings.fileName().contains(swappedName));
            CHECK(settings.fileName().contains(QStringLiteral("/cavewhere-test-settings-")));
        }

        QCoreApplication::setApplicationName(originalName);
    }
}
