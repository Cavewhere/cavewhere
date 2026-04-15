//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "TestHelper.h"
#include "cwRootData.h"
#include "cwPDFSettings.h"
#include "cwPDFConverter.h"
#include "cwScrapManager.h"
#include "cwNote.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyNoteModel.h"
#include "cwGeometry.h"
#include "cwScrap.h"
#include "cwCavingRegion.h"
#include "cwTriangulateTask.h"
#include "cwTriangulateInData.h"
#include "cwTriangulatedData.h"
#include "cwImageUtils.h"
#include "cwNoteStation.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyNetwork.h"
#include "cwPlanScrapViewMatrix.h"
#include "cwUnits.h"
#include "cwTextureUploadTask.h"
#include "asyncfuture.h"

//Qt includes
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QPainter>
#include <QCoreApplication>

//Std includes
#include <algorithm>

static QSizeF geometryBoundsSize(const cwGeometry& geometry)
{
    const QVector<QVector3D> positions = geometry.values<QVector3D>(cwGeometry::Semantic::Position);
    if (positions.isEmpty()) {
        return {};
    }

    QVector3D min = positions.first();
    QVector3D max = positions.first();
    for (const QVector3D& p : positions) {
        min.setX(std::min(min.x(), p.x()));
        min.setY(std::min(min.y(), p.y()));
        min.setZ(std::min(min.z(), p.z()));
        max.setX(std::max(max.x(), p.x()));
        max.setY(std::max(max.y(), p.y()));
        max.setZ(std::max(max.z(), p.z()));
    }

    return QSizeF(max.x() - min.x(), max.y() - min.y());
}

TEST_CASE("cwTriangulateTask uses note units for crop scaling", "[cwTriangulateTask]") {
    cwPDFSettings::initialize();
    if (!cwPDFConverter::isSupported()) {
        SKIP("PDF support not enabled for cwTriangulateTask test");
    }

    auto triangulateBounds = [](int resolutionPpi) {
        cwPDFSettings::instance()->setResolutionImport(resolutionPpi);

        auto root = std::make_unique<cwRootData>();
        TestHelper helper;
        helper.loadProjectFromZip(root->project(), "://datasets/test_cwNote/bb-pdf-dpi-test.zip");

        auto project = root->project();
        REQUIRE(project->cavingRegion()->caveCount() == 1);
        auto cave = project->cavingRegion()->cave(0);
        REQUIRE(cave->tripCount() == 1);
        auto trip = cave->trip(0);
        auto notes = trip->notes()->notes();
        REQUIRE(notes.size() >= 2);
        cwNote* note = notes.at(1);
        REQUIRE(note != nullptr);
        REQUIRE(note->scraps().size() >= 1);
        cwScrap* scrap = note->scrap(0);
        REQUIRE(scrap != nullptr);

        auto results = root->scrapManager()->triangulateScraps({scrap});
        root->futureManagerModel()->waitForFinished();
        REQUIRE(AsyncFuture::waitForFinished(results.first().data));
        REQUIRE(results.size() == 1);
        REQUIRE(results.first().data.resultCount() == 1);
        const cwTriangulatedData data = results.first().data.result();
        const QSizeF bounds = geometryBoundsSize(data.scrapGeometry());
        REQUIRE(bounds.isValid());
        return bounds;
    };

    const QSizeF bounds300 = triangulateBounds(300);
    const QSizeF bounds100 = triangulateBounds(100);

    CHECK(bounds300.width() == Catch::Approx(bounds100.width()).epsilon(1e-4));
    CHECK(bounds300.height() == Catch::Approx(bounds100.height()).epsilon(1e-4));
}

static QString testDatasetDir()
{
    return QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath())
        .filePath(QStringLiteral("datasets/test_cwTriangulateTask"));
}

static QSize rawPixelSize(const QString& imagePath)
{
    QImageReader reader(imagePath);
    return reader.size();
}

static cwTriangulatedData triangulateImageAtPath(const QString& imagePath,
                                                  QSize overrideOriginalSize = QSize())
{
    QFile file(imagePath);
    REQUIRE(file.open(QIODevice::ReadOnly));
    QByteArray rawData = file.readAll();
    file.close();
    const QByteArray format = QFileInfo(imagePath).suffix().toLatin1();
    QImage qimage = cwImageUtils::imageWithAutoTransform(rawData, format);
    REQUIRE(!qimage.isNull());
    const QSize displayedSize = overrideOriginalSize.isValid()
                                    ? overrideOriginalSize
                                    : qimage.size();
    INFO("Displayed size: " << displayedSize.width() << "x" << displayedSize.height());

    cwImage noteImage;
    noteImage.setPath(imagePath);
    noteImage.setOriginalSize(displayedSize);
    noteImage.setOriginalDotsPerMeter(qimage.dotsPerMeterX() > 0
                                          ? qimage.dotsPerMeterX()
                                          : qRound(300.0 / 0.0254));

    cwTriangulateInData inData;
    inData.setNoteImage(noteImage);

    QPolygonF outline;
    outline << QPointF(0.1, 0.1) << QPointF(0.9, 0.1)
            << QPointF(0.9, 0.9) << QPointF(0.1, 0.9);
    inData.setOutline(outline);

    cwNoteStation s1;
    s1.setName(QStringLiteral("a1"));
    s1.setPositionOnNote(QPointF(0.2, 0.2));
    cwNoteStation s2;
    s2.setName(QStringLiteral("a2"));
    s2.setPositionOnNote(QPointF(0.8, 0.8));
    inData.setNoteStation({s1, s2});

    cwStationPositionLookup lookup;
    lookup.setPosition(QStringLiteral("a1"), QVector3D(0, 0, 0));
    lookup.setPosition(QStringLiteral("a2"), QVector3D(10, 10, 0));
    inData.setStationLookup(lookup);

    cwSurveyNetwork network;
    network.addShot(QStringLiteral("a1"), QStringLiteral("a2"));
    inData.setSurveyNetwork(network);

    cwNoteTransformationData transform;
    transform.north = 0.0;
    transform.scale.scaleNumerator = {cwUnits::Meters, 1.0, false};
    transform.scale.scaleDenominator = {cwUnits::Meters, 100.0, false};
    inData.setNoteTransform(transform);

    inData.setViewMatrix(new cwPlanScrapViewMatrix::Data());
    inData.setNoteImageResolution(qRound(300.0 / 0.0254));

    const QDir tempDir(QDir::temp().filePath(
        QStringLiteral("cwTriangulateTask-%1").arg(QCoreApplication::applicationPid())));
    tempDir.mkpath(".");

    cwTriangulateTask task;
    task.setScrapData({inData});
    task.setDataRootDir(tempDir);
    task.setFormatType(cwTextureUploadTask::OpenGL_RGBA);
    auto futures = task.triangulate();
    REQUIRE(futures.size() == 1);
    REQUIRE(AsyncFuture::waitForFinished(futures.first()));
    REQUIRE(futures.first().resultCount() == 1);

    return futures.first().result();
}

TEST_CASE("cwTriangulateTask produces consistent geometry for EXIF-rotated images", "[cwTriangulateTask]") {
    const QString dataDir = testDatasetDir();
    const QString noRotationPath = QDir(dataDir).filePath(QStringLiteral("scrap-image-no-rotation.jpg"));
    const QString rotation90Path = QDir(dataDir).filePath(QStringLiteral("scrap-image-rotation90.jpg"));
    REQUIRE(QFileInfo::exists(noRotationPath));
    REQUIRE(QFileInfo::exists(rotation90Path));

    const cwTriangulatedData noRotResult = triangulateImageAtPath(noRotationPath);
    const cwTriangulatedData rotResult = triangulateImageAtPath(rotation90Path);

    REQUIRE(!noRotResult.isNull());
    REQUIRE(!rotResult.isNull());

    const cwGeometry& noRotGeom = noRotResult.scrapGeometry();
    const cwGeometry& rotGeom = rotResult.scrapGeometry();

    SECTION("Geometry vertex counts match") {
        const auto noRotPositions = noRotGeom.values<QVector3D>(cwGeometry::Semantic::Position);
        const auto rotPositions = rotGeom.values<QVector3D>(cwGeometry::Semantic::Position);
        CHECK(noRotPositions.size() == rotPositions.size());
    }

    SECTION("Geometry bounds match") {
        const QSizeF noRotBounds = geometryBoundsSize(noRotGeom);
        const QSizeF rotBounds = geometryBoundsSize(rotGeom);
        REQUIRE(noRotBounds.isValid());
        REQUIRE(rotBounds.isValid());
        CHECK(noRotBounds.width() == Catch::Approx(rotBounds.width()).epsilon(1e-4));
        CHECK(noRotBounds.height() == Catch::Approx(rotBounds.height()).epsilon(1e-4));
    }

    SECTION("Texture coordinates match") {
        const auto noRotTex = noRotGeom.values<QVector2D>(cwGeometry::Semantic::TexCoord0);
        const auto rotTex = rotGeom.values<QVector2D>(cwGeometry::Semantic::TexCoord0);
        REQUIRE(noRotTex.size() == rotTex.size());
        for (int i = 0; i < noRotTex.size(); ++i) {
            INFO("Vertex " << i);
            CHECK(noRotTex[i].x() == Catch::Approx(rotTex[i].x()).epsilon(1e-4));
            CHECK(noRotTex[i].y() == Catch::Approx(rotTex[i].y()).epsilon(1e-4));
        }
    }

    SECTION("Texture coordinates are in [0,1] range") {
        const auto rotTex = rotGeom.values<QVector2D>(cwGeometry::Semantic::TexCoord0);
        REQUIRE(!rotTex.isEmpty());
        for (int i = 0; i < rotTex.size(); ++i) {
            INFO("Vertex " << i);
            CHECK(rotTex[i].x() >= -0.01f);
            CHECK(rotTex[i].x() <= 1.01f);
            CHECK(rotTex[i].y() >= -0.01f);
            CHECK(rotTex[i].y() <= 1.01f);
        }
    }

    SECTION("Cropped image dimensions match") {
        const QSize noRotCropSize = noRotResult.croppedImage().originalSize();
        const QSize rotCropSize = rotResult.croppedImage().originalSize();
        CHECK(noRotCropSize == rotCropSize);
    }
}

TEST_CASE("cwTriangulateTask detects pre-rotation originalSize mismatch", "[cwTriangulateTask]") {
    const QString dataDir = testDatasetDir();
    const QString noRotationPath = QDir(dataDir).filePath(QStringLiteral("scrap-image-no-rotation.jpg"));
    const QString rotation90Path = QDir(dataDir).filePath(QStringLiteral("scrap-image-rotation90.jpg"));
    REQUIRE(QFileInfo::exists(noRotationPath));
    REQUIRE(QFileInfo::exists(rotation90Path));

    // The rotated image has raw pixels 40x60 but displays as 60x40 after auto-transform.
    // If originalSize is incorrectly set to the pre-rotation size (40x60), the grid
    // and morphing calculations use wrong physical dimensions, producing wrong geometry.
    const QSize preRotationSize = rawPixelSize(rotation90Path);
    INFO("Pre-rotation (raw) size: " << preRotationSize.width() << "x" << preRotationSize.height());
    REQUIRE(preRotationSize.width() != preRotationSize.height());

    const cwTriangulatedData correctResult = triangulateImageAtPath(noRotationPath);
    const cwTriangulatedData buggyResult = triangulateImageAtPath(rotation90Path, preRotationSize);

    REQUIRE(!correctResult.isNull());
    REQUIRE(!buggyResult.isNull());

    const QSizeF correctBounds = geometryBoundsSize(correctResult.scrapGeometry());
    const QSizeF buggyBounds = geometryBoundsSize(buggyResult.scrapGeometry());
    REQUIRE(correctBounds.isValid());
    REQUIRE(buggyBounds.isValid());

    // When originalSize is wrong (pre-rotation), the geometry bounds should differ
    // because createPointGrid and morphPoints use originalSize to compute physical sizes.
    const bool widthMatches = std::abs(correctBounds.width() - buggyBounds.width())
                              < correctBounds.width() * 0.01;
    const bool heightMatches = std::abs(correctBounds.height() - buggyBounds.height())
                               < correctBounds.height() * 0.01;
    CHECK_FALSE((widthMatches && heightMatches));
}
