//Catch includes
#include "catch.hpp"

//Our includes
#include "cwTexture.h"
#include "cwOpenGLSettings.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "TestHelper.h"
#include "cwFutureManagerModel.h"
#include "cwAsyncFuture.h"

//Qt3D inculdes
#include <QAbstractTexture>

TEST_CASE("cwTexture should update mag filter correctly", "[cwTexture]") {

    cwOpenGLSettings::instance()->setToDefault();

    cwRootData root;
    auto project = root.project();

    QString filename = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    REQUIRE(project->filename().isEmpty() == false);

    bool ran = false;

    auto settings = cwOpenGLSettings::instance();

    SECTION("DXT1") {
        settings->setUseDXT1Compression(true);
    }

    SECTION("RGBA") {
        settings->setUseDXT1Compression(false);
    }

    project->addImages({QUrl::fromLocalFile(filename)},
                       [&ran, &root, settings, project](QList<cwImage> images)
    {
        REQUIRE(images.size() == 1);

        cwTexture texture;
        texture.setFutureToken(root.futureManagerModel()->token());
        texture.setProject(project->filename());
        texture.setImage(images.first());

        auto toMagFilter = [](cwOpenGLSettings::MagFilter filter) {
            switch(filter) {
            case cwOpenGLSettings::MagNearest:
                return Qt3DRender::QAbstractTexture::Nearest;
            case cwOpenGLSettings::MagLinear:
                return Qt3DRender::QAbstractTexture::Linear;
            }
            REQUIRE(false);
            return Qt3DRender::QAbstractTexture::Nearest;
        };

        settings->setMagFilter(cwOpenGLSettings::MagLinear);
        root.futureManagerModel()->waitForFinished();
        CHECK(texture.texture()->magnificationFilter() == toMagFilter(settings->magFilter()));

        settings->setMagFilter(cwOpenGLSettings::MagNearest);
        root.futureManagerModel()->waitForFinished();
        CHECK(texture.texture()->magnificationFilter() == toMagFilter(settings->magFilter()));

        ran = true;
    });

    root.futureManagerModel()->waitForFinished();
    CHECK(ran);


}

TEST_CASE("cwTexture should update min filter correctly", "[cwTexture]") {

     cwOpenGLSettings::instance()->setToDefault();

    auto settings = cwOpenGLSettings::instance();

    auto toMinFilter = [](cwOpenGLSettings::MinFilter filter) {
        switch(filter) {
        case cwOpenGLSettings::MinLinear:
            return Qt3DRender::QAbstractTexture::Linear;
        case cwOpenGLSettings::MinNearest_Mipmap_Linear:
            return Qt3DRender::QAbstractTexture::NearestMipMapLinear;
        case cwOpenGLSettings::MinLinear_Mipmap_Linear:
            return Qt3DRender::QAbstractTexture::LinearMipMapLinear;
        }
        REQUIRE(false);
        return Qt3DRender::QAbstractTexture::Nearest;
    };

    cwRootData root;
    auto project = root.project();


    QString filename = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    REQUIRE(project->filename().isEmpty() == false);

    bool ran = false;

    SECTION("DXT1") {
        settings->setUseDXT1Compression(true);
    }

    SECTION("RGBA") {
        settings->setUseDXT1Compression(false);
    }

    project->addImages({QUrl::fromLocalFile(filename)},
                       [settings, toMinFilter, project, &root, &ran](QList<cwImage> images)
    {
        Q_UNUSED(images);
        cwTexture texture;
        texture.setFutureToken(root.futureManagerModel()->token());
        texture.setProject(project->filename());
        texture.setImage(images.first());

        settings->setMinFilter(cwOpenGLSettings::MinLinear);
        root.futureManagerModel()->waitForFinished();
        CHECK(texture.texture()->minificationFilter() == toMinFilter(settings->minFilter()));

        settings->setMinFilter(cwOpenGLSettings::MinNearest_Mipmap_Linear);
        root.futureManagerModel()->waitForFinished();
        CHECK(texture.texture()->minificationFilter() == toMinFilter(settings->minFilter()));

        settings->setMinFilter(cwOpenGLSettings::MinLinear_Mipmap_Linear);
        root.futureManagerModel()->waitForFinished();
        CHECK(texture.texture()->minificationFilter() == toMinFilter(settings->minFilter()));

        settings->setMinFilter(cwOpenGLSettings::MinLinear);
        root.futureManagerModel()->waitForFinished();
        CHECK(texture.texture()->minificationFilter() == toMinFilter(settings->minFilter()));

        ran = true;
    });

    root.futureManagerModel()->waitForFinished();
    CHECK(ran);


}

TEST_CASE("cwTexture should update anisotropy correctly", "[cwTexture]") {

    cwOpenGLSettings::instance()->setToDefault();

    auto settings = cwOpenGLSettings::instance();

    auto anisotropySetting = [settings]() {
        return settings->useAnisotropy() ? 16.0f : 1.0f;
    };

    cwRootData root;
    auto project = root.project();

    QString filename = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    REQUIRE(project->filename().isEmpty() == false);

    bool ran = false;

    SECTION("DXT1") {
        settings->setUseDXT1Compression(true);
    }

    SECTION("RGBA") {
        settings->setUseDXT1Compression(false);
    }

    project->addImages({QUrl::fromLocalFile(filename)},
                       [settings, anisotropySetting, project, &root, &ran](QList<cwImage> images)
    {
        cwTexture texture;
        texture.setFutureToken(root.futureManagerModel()->token());
        texture.setProject(project->filename());
        texture.setImage(images.first());

        settings->setUseAnisotropy(false);
        root.futureManagerModel()->waitForFinished();
        CHECK(texture.texture()->maximumAnisotropy() == anisotropySetting());

        settings->setUseAnisotropy(true);
        root.futureManagerModel()->waitForFinished();
        CHECK(texture.texture()->maximumAnisotropy() == anisotropySetting());

        ran = true;
    });

    root.futureManagerModel()->waitForFinished();
    CHECK(ran);


}

TEST_CASE("cwTexture should update mipmap correctly", "[cwTexture]") {
     cwOpenGLSettings::instance()->setToDefault();

    cwRootData root;
    auto project = root.project();

    QString filename = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    REQUIRE(project->filename().isEmpty() == false);

    bool ran = false;

    cwOpenGLSettings::instance()->setUseDXT1Compression(true);
    cwOpenGLSettings::instance()->setMipmaps(true);
    cwOpenGLSettings::instance()->setMinFilter(cwOpenGLSettings::MinLinear);

    project->addImages({QUrl::fromLocalFile(filename)},
                       [project, &root, &ran](QList<cwImage> images)
    {
        REQUIRE(images.size() == 1);

        auto image = images.first();
        REQUIRE(image.isOriginalValid() == true);



        cwTexture texture;
        texture.setFutureToken(root.futureManagerModel()->token());
        texture.setProject(project->filename());
        texture.setImage(image);

        root.futureManagerModel()->waitForFinished();

        CHECK(texture.future().isFinished());
        REQUIRE(texture.future().isCanceled() == false);

        CHECK(texture.texture()->textureImages().size() == 10);
        CHECK(texture.texture()->generateMipMaps() == false);

        cwOpenGLSettings::instance()->setMinFilter(cwOpenGLSettings::MinLinear_Mipmap_Linear);

        cwOpenGLSettings::instance()->setMipmaps(false);
        root.futureManagerModel()->waitForFinished();

        CHECK(texture.future().isFinished());
        REQUIRE(texture.future().isCanceled() == false);

        CHECK(texture.texture()->textureImages().size() == 1);
        CHECK(texture.texture()->generateMipMaps() == false);
        CHECK(texture.texture()->minificationFilter() == Qt3DRender::QAbstractTexture::Linear);

        cwOpenGLSettings::instance()->setUseDXT1Compression(false);
        cwOpenGLSettings::instance()->setMipmaps(true);
        root.futureManagerModel()->waitForFinished();

        REQUIRE(texture.texture()->textureImages().size() == 1);
        CHECK(texture.texture()->generateMipMaps() == true);
        CHECK(texture.texture()->minificationFilter() == Qt3DRender::QAbstractTexture::LinearMipMapLinear);

        ran = true;
    });

    root.futureManagerModel()->waitForFinished();
    CHECK(ran);


}
