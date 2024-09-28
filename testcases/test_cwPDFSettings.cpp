//Our includes
#include "cwPDFSettings.h"
#include "SpyChecker.h"
#include "cwPDFConverter.h"

//Qt includes
#include <QSettings>
#include "cwSignalSpy.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>

TEST_CASE("cwPDFSettings should ", "[cwPDFSettings]") {
    QSettings diskSettings;

    cwPDFSettings::initialize();
    auto settings = cwPDFSettings::instance();
    cwPDFSettings::initialize();
    auto settings2 = cwPDFSettings::instance();

    CHECK(settings == settings2);

    cwSignalSpy resolutionSpy(settings, &cwPDFSettings::resolutionImportChanged);

    SpyChecker checker = {
        {&resolutionSpy, 0}
    };

    CHECK(settings->isSupportImport() == cwPDFConverter::isSupported());

    SECTION("Check resolution change") {
        settings->setResolutionImport(80);
        checker[&resolutionSpy]++;
        checker.checkSpies();
        CHECK(diskSettings.value("importResolutionInPPI").toInt() == settings->resolutionImport());
    }
}
