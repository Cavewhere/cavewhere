//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Qt includes
#include <QBuffer>
#include <QCoreApplication>
#include <QFileInfo>
#include <QImageReader>

//Our includes
#include "TestHelper.h"
#include "cwRootData.h"
#include "cwProject.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyNoteModel.h"
#include "cwNote.h"
#include "cwScrap.h"
#include "cwScrapView.h"
#include "cwScrapItem.h"
#include "cwImage.h"
#include "cwSvgReader.h"
#include "cwPDFSettings.h"
#include "cwUnits.h"
#include "cwFutureManagerModel.h"

#include <cmath>
#include <memory>

namespace {

// 256 MB / 4 bytes-per-pixel = max pixel count before clampImageSize kicks in
constexpr qint64 kMaxPixels = (256ll * 1024 * 1024) / 4;

//! Build a minimal SVG with only a viewBox (no explicit width/height).
//! The viewBox dimensions control the native size at 96 DPI.
QByteArray buildViewBoxSvg(int viewBoxWidth, int viewBoxHeight)
{
    return QStringLiteral(
               R"(<?xml version="1.0" encoding="UTF-8"?>)"
               R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %1 %2">)"
               R"(<rect width="%1" height="%2" fill="white"/>)"
               R"(</svg>)")
        .arg(viewBoxWidth)
        .arg(viewBoxHeight)
        .toLatin1();
}

//! Write SVG data to the temp folder and return the path.
QString writeSvgToTemp(const QByteArray& svgData, const QString& name)
{
    QFileInfo info(name);
    const QString uniqueName = QStringLiteral("%1-%2.%3")
        .arg(info.completeBaseName())
        .arg(QCoreApplication::applicationPid())
        .arg(info.suffix());
    const QString path = prependTempFolder(uniqueName);
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write(svgData);
    file.close();
    return path;
}

//! Import an SVG into a new project and return the note.
struct SvgNoteFixture {
    std::unique_ptr<cwRootData> root;
    cwNote* note = nullptr;

    SvgNoteFixture(const QString& svgFilePath)
        : root(std::make_unique<cwRootData>())
    {
        auto* project = root->project();
        auto* region = project->cavingRegion();
        region->addCave();
        auto* cave = region->cave(0);
        cave->addTrip();
        auto* trip = cave->trip(0);

        trip->notes()->addFromFiles({QUrl::fromLocalFile(svgFilePath)});
        root->futureManagerModel()->waitForFinished();

        auto notes = trip->notes()->notes();
        REQUIRE(notes.size() == 1);
        note = notes.first();
        REQUIRE(note != nullptr);
    }
};

//! Return the actual pixel size that cwSvgReader::toImage produces.
QSize actualSvgImageSize(QByteArray svgData, int dpi)
{
    QByteArray format = "svg";
    cwPDFSettings::instance()->setResolutionImport(dpi);
    QImage image = cwSvgReader::toImage(svgData, format);
    REQUIRE(!image.isNull());
    return image.size();
}

} // namespace

TEST_CASE("renderSize accounts for image clamping on large SVGs at high DPI",
          "[cwScrapSvgDpi]")
{
    cwPDFSettings::initialize();

    // Choose a viewBox large enough that at 300 DPI the pixel count exceeds
    // the 256 MB cap, triggering clampImageSize inside cwSvgReader::toImage().
    //
    // viewBox 2282×3515  →  at 300 DPI: 7131×10984 = 78.3M pixels > 67.1M max
    //                    →  at  96 DPI: 2282×3515  =  8.0M pixels  (no clamp)
    const int vbW = 2282;
    const int vbH = 3515;
    const QByteArray svgData = buildViewBoxSvg(vbW, vbH);
    const QString svgPath = writeSvgToTemp(svgData, QStringLiteral("large_note.svg"));

    SECTION("At 96 DPI — no clamping, renderSize matches image") {
        cwPDFSettings::instance()->setResolutionImport(96);
        SvgNoteFixture fixture(svgPath);

        const QSize renderSize = fixture.note->renderSize();
        const QSize imageSize = actualSvgImageSize(svgData, 96);

        // Verify clamping is NOT active at 96 DPI
        const qint64 pixels = static_cast<qint64>(imageSize.width()) * imageSize.height();
        REQUIRE(pixels <= kMaxPixels);

        INFO("renderSize = " << renderSize.width() << "x" << renderSize.height());
        INFO("imageSize  = " << imageSize.width() << "x" << imageSize.height());

        CHECK(renderSize == imageSize);
    }

    SECTION("At 300 DPI — clamping active, renderSize must still match image") {
        cwPDFSettings::instance()->setResolutionImport(300);
        SvgNoteFixture fixture(svgPath);

        const QSize renderSize = fixture.note->renderSize();
        const QSize imageSize = actualSvgImageSize(svgData, 300);

        // Verify clamping IS active at 300 DPI
        const double scale = 300.0 / cwUnits::SvgCssDpi;
        const qint64 unclampedPixels =
            static_cast<qint64>(qRound(vbW * scale)) * qRound(vbH * scale);
        REQUIRE(unclampedPixels > kMaxPixels);

        INFO("unclamped renderSize = " << renderSize.width() << "x" << renderSize.height());
        INFO("clamped imageSize    = " << imageSize.width() << "x" << imageSize.height());

        // This is the bug: renderSize returns the unclamped size while
        // the image provider delivers the clamped size. The scrap outline
        // is drawn in renderSize coordinates over the smaller clamped image,
        // causing a visible ~8% offset.
        CHECK(renderSize == imageSize);
    }
}

TEST_CASE("Scrap outline coordinates stay within actual image bounds after clamping",
          "[cwScrapSvgDpi]")
{
    cwPDFSettings::initialize();
    cwPDFSettings::instance()->setResolutionImport(300);

    const QByteArray svgData = buildViewBoxSvg(2282, 3515);
    const QString svgPath = writeSvgToTemp(svgData, QStringLiteral("large_note_scrap.svg"));
    SvgNoteFixture fixture(svgPath);

    // Add a scrap that spans most of the note
    auto* scrap = new cwScrap();
    scrap->addPoint(QPointF(0.05, 0.05));
    scrap->addPoint(QPointF(0.95, 0.05));
    scrap->addPoint(QPointF(0.95, 0.95));
    scrap->addPoint(QPointF(0.05, 0.95));
    fixture.note->addScrap(scrap);

    const QSize imageSize = actualSvgImageSize(svgData, 300);
    const QTransform toImage = cwScrapView::toImage(fixture.note);

    INFO("renderSize = " << fixture.note->renderSize().width() << "x"
                         << fixture.note->renderSize().height());
    INFO("imageSize  = " << imageSize.width() << "x" << imageSize.height());

    // Every scrap outline point, when mapped to image coordinates via toImage(),
    // must land within the actual displayed image bounds.
    for (int i = 0; i < scrap->numberOfPoints(); ++i) {
        const QPointF normalized = scrap->points().at(i);
        const QPointF imagePt = toImage.map(normalized);
        CAPTURE(i, normalized, imagePt);
        CHECK(imagePt.x() >= 0.0);
        CHECK(imagePt.y() >= 0.0);
        CHECK(imagePt.x() <= static_cast<double>(imageSize.width()));
        CHECK(imagePt.y() <= static_cast<double>(imageSize.height()));
    }
}
