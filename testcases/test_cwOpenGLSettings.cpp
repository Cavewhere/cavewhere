//Catch includes
#include "catch.hpp"

//Our includes
#include "cwOpenGLSettings.h"
#include "SpyChecker.h"

//Qt includes
#include <QRegularExpression>
#include <QSettings>
#include <QSignalSpy>

TEST_CASE("cwOpenGLSettings should initilize correctly", "[cwOpenGLSettings]") {
    auto checkUserDefaults = [](const cwOpenGLSettings& settings) {
        if(settings.dxt1Supported()) {
            CHECK(settings.useDXT1Compression() == true);
        } else {
            CHECK(settings.useDXT1Compression() == false);
        }

        if(settings.anisotropySupported()) {
            CHECK(settings.useAnisotropy() == true);
        } else {
            CHECK(settings.useAnisotropy() == false);
        }
        CHECK(settings.useMipmaps() == true);
        CHECK(settings.useNativeTextRendering() == false);
        CHECK(settings.minFilter() == cwOpenGLSettings::MinNearest_Mipmap_Linear);
        CHECK(settings.magFilter() == cwOpenGLSettings::MagNearest);
        CHECK(settings.dxt1Algorithm() == cwOpenGLSettings::DXT1_Squish);
    };

    SECTION("Singleton initilization should work correctly") {
        QSettings settings;
        settings.clear();

        cwOpenGLSettings::initialize();
        cwOpenGLSettings* initSettings = cwOpenGLSettings::instance();

        checkUserDefaults(*initSettings);

        QSignalSpy restartSpy(initSettings, &cwOpenGLSettings::needsRestartChanged);
        QSignalSpy dxt1ComprossedSpy(initSettings, &cwOpenGLSettings::useDXT1CompressionChanged);
        QSignalSpy mipmapsSpy(initSettings, &cwOpenGLSettings::useMipmapsChanged);
        QSignalSpy aniotropySpy(initSettings, &cwOpenGLSettings::useAnisotropyChanged);
        QSignalSpy nativeTextRenderingSpy(initSettings, &cwOpenGLSettings::useNativeTextRenderingChanged);
        QSignalSpy magFilterSpy(initSettings, &cwOpenGLSettings::magFilterChanged);
        QSignalSpy minFilterSpy(initSettings, &cwOpenGLSettings::minFilterChanged);
        QSignalSpy dxt1AlgoSpy(initSettings, &cwOpenGLSettings::dxt1AlgorithmChanged);

        restartSpy.setObjectName("restartSpy");
        dxt1ComprossedSpy.setObjectName("dxt1CompressedSpy");
        mipmapsSpy.setObjectName("mipmapSpy");
        aniotropySpy.setObjectName("aniotropySpy");
        nativeTextRenderingSpy.setObjectName("nativeTextRenderingSpy");
        magFilterSpy.setObjectName("magFilterSpy");
        minFilterSpy.setObjectName("minFilterSpy");
        dxt1AlgoSpy.setObjectName("dxt1AlgoSpy");

        SpyChecker checker = {
            {&restartSpy, 0},
            {&dxt1ComprossedSpy, 0},
            {&mipmapsSpy, 0},
            {&aniotropySpy, 0},
            {&nativeTextRenderingSpy, 0},
            {&magFilterSpy, 0},
            {&minFilterSpy, 0},
            {&dxt1AlgoSpy, 0}
        };

        CHECK(initSettings->vendor().isEmpty() == false);
        CHECK(initSettings->version().isEmpty() == false);
        CHECK(initSettings->renderer().isEmpty() == false);
        CHECK(initSettings->shadingVersion().isEmpty() == false);
        CHECK(initSettings->extentions().isEmpty() == false);

        auto checkVersion = [](const QString& version) {
            INFO("Version:" << version.toStdString())
                    QRegularExpression versionExpression("^\\d+\\.\\d+");
            auto match = versionExpression.match(version);
            CHECK(match.hasMatch());
        };

        checkVersion(initSettings->version());
        checkVersion(initSettings->shadingVersion());

        SECTION("Make sure that the previousRenderer works correctly") {

            CHECK(cwOpenGLSettings::perviousRenderer() == cwOpenGLSettings::Auto);

            //Simplate a crash when checking
            QString crashWhenTestingKey = cwOpenGLSettings::key("testing");
            settings.setValue(crashWhenTestingKey, true);
            CHECK(cwOpenGLSettings::perviousRenderer() == cwOpenGLSettings::Software);

            settings.setValue(crashWhenTestingKey, false);
            initSettings->setRendererType(cwOpenGLSettings::GPU);
            checker[&restartSpy]++;
            checker.checkSpies();

            CHECK(settings.value(crashWhenTestingKey).toBool() == false);

            CHECK(cwOpenGLSettings::perviousRenderer() == cwOpenGLSettings::GPU);
        }

        auto repeat = [&](std::function<void ()> func) {
            for(int i = 0; i < 2; i++) {
                func();
            }
        };

        SECTION("Make sure init works correctly from settings") {

            SECTION("DXT1 Compression") {
                if(initSettings->dxt1Supported()) {
                    repeat([&]() {
                        initSettings->setDXT1Compression(!initSettings->useDXT1Compression());
                        checker[&dxt1ComprossedSpy]++;

                        CHECK(initSettings->useDXT1Compression() == settings.value(initSettings->keyWithDevice("useDxt1Compression")).toBool());

                        checker.checkSpies();
                    });
                }
            }

            SECTION("Anisotropy Supported") {
                if(initSettings->anisotropySupported()) {
                    repeat([&]() {
                        initSettings->setUseAnisotropy(!initSettings->useAnisotropy());

                        checker[&aniotropySpy]++;

                        CHECK(initSettings->useAnisotropy() == settings.value(initSettings->keyWithDevice("useAnisotropy")).toBool());

                        checker.checkSpies();
                    });
                }
            }

            SECTION("Mipmaps") {
                repeat([&]() {
                    initSettings->setMipmaps(!initSettings->useMipmaps());
                    checker[&mipmapsSpy]++;

                    CHECK(initSettings->useMipmaps() == settings.value(initSettings->keyWithDevice("useMipmaps")).toBool());

                    checker.checkSpies();
                });
            }

            SECTION("Native Text Rendering") {
                repeat([&]() {
                    initSettings->setNativeTextRendering(!initSettings->useNativeTextRendering());
                    checker[&nativeTextRenderingSpy]++;
                    checker[&restartSpy]++;

                    CHECK(initSettings->useNativeTextRendering() == settings.value(initSettings->keyWithDevice("useNativeTextRendering")).toBool());

                    checker.checkSpies();
                });
            }

            SECTION("MagFilter") {
                initSettings->setMagFilter(cwOpenGLSettings::MagLinear);
                checker[&magFilterSpy]++;

                CHECK(initSettings->magFilter() == settings.value(initSettings->keyWithDevice("scrapMagFilter")).toInt());

                checker.checkSpies();
            }

            SECTION("MinFilter") {
                initSettings->setMinFilter(cwOpenGLSettings::MinLinear_Mipmap_Linear);
                checker[&minFilterSpy]++;

                CHECK(initSettings->minFilter() == settings.value(initSettings->keyWithDevice("scrapMinFilter")).toInt());

                checker.checkSpies();
            }

            SECTION("DXT1 Algorithm") {
                if(initSettings->gpuGeneratedDXT1Supported()) {
                    initSettings->setDXT1Algorithm(cwOpenGLSettings::DXT1_GPU);
                    checker[&dxt1AlgoSpy]++;

                    CHECK(initSettings->dxt1Algorithm() == settings.value(initSettings->keyWithDevice("DXT1GenerateAlgroKey")).toInt());

                    checker.checkSpies();
                }
            }

            SECTION("Check that the settings load correctly on init") {
                initSettings->setToDefault();

                if(initSettings->gpuGeneratedDXT1Supported()) {
                    initSettings->setDXT1Algorithm(cwOpenGLSettings::DXT1_GPU);
                }
                if(initSettings->dxt1Supported()) {
                    initSettings->setDXT1Compression(false);
                }
                if(initSettings->anisotropySupported()) {
                    initSettings->setUseAnisotropy(false);
                }

                initSettings->setMipmaps(false);
                initSettings->setMagFilter(cwOpenGLSettings::MagLinear);
                initSettings->setMinFilter(cwOpenGLSettings::MinLinear_Mipmap_Linear);
                initSettings->setRendererType(cwOpenGLSettings::Software);
                initSettings->setNativeTextRendering(true);

                initSettings->cleanup();

                cwOpenGLSettings::initialize();
                initSettings = cwOpenGLSettings::instance();

                if(initSettings->gpuGeneratedDXT1Supported()) {
                    CHECK(initSettings->dxt1Algorithm() == cwOpenGLSettings::DXT1_GPU);
                }
                if(initSettings->dxt1Supported()) {
                    CHECK(initSettings->useDXT1Compression() == false);
                }
                if(initSettings->anisotropySupported()) {
                    CHECK(initSettings->useAnisotropy() == false);
                }

                CHECK(initSettings->useMipmaps() == false);
                CHECK(initSettings->magFilter() == cwOpenGLSettings::MagLinear);
                CHECK(initSettings->minFilter() == cwOpenGLSettings::MinLinear_Mipmap_Linear);
                CHECK(initSettings->rendererType() == cwOpenGLSettings::Software);
                CHECK(initSettings->useNativeTextRendering() == true);
            }
        }

        settings.clear();
        initSettings->cleanup();
    }
}
