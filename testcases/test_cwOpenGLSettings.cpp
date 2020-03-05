//Catch includes
#include "catch.hpp"

//Our includes
#include "cwOpenGLSettings.h"

TEST_CASE("cwOpenGLSettings should initilize correctly", "[cwOpenGLSettings]") {

    cwOpenGLSettings settings;
    CHECK(settings.dxt1Supported() == false);
    CHECK(settings.anisotropySupported() == false);
    CHECK(settings.useDXT1Compression() == false);
    CHECK(settings.useAnisotropy() == false);
    CHECK(settings.useMipmaps() == true);
    CHECK(settings.useNativeTextRendering() == false);
    CHECK(settings.rendererType() == cwOpenGLSettings::Auto);
    CHECK(settings.needsRestart() == false);
    CHECK(settings.vendor().toStdString() == "");
    CHECK(settings.version().toStdString() == "");
    CHECK(settings.renderer().toStdString() == "");
    CHECK(settings.shadingVersion().toStdString() == "");
    CHECK(settings.extentions().isEmpty() == true);
    CHECK(settings.minFilter() == cwOpenGLSettings::MinNearest_Mipmap_Linear);
    CHECK(settings.magFilter() == cwOpenGLSettings::MagNearest);
    CHECK(settings.gpuGeneratedDXT1Supported() == false);
    CHECK(settings.dxt1Algorithm() == cwOpenGLSettings::DXT1_Squish);


    SECTION("Singleton initilization should work correctly") {
        cwOpenGLSettings::initialize();
        cwOpenGLSettings* initSettings = cwOpenGLSettings::instance();

        CHECK(initSettings->vendor().isEmpty() == false);
        CHECK(initSettings->version().isEmpty() == false);
        CHECK(initSettings->renderer().isEmpty() == false);
        CHECK(initSettings->shadingVersion().isEmpty() == false);
        CHECK(initSettings->extentions().isEmpty() == false);

    }




}
