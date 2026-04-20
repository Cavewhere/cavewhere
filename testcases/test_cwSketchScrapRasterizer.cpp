//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QColor>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QRgb>

//Our includes
#include "cwLength.h"
#include "cwPenPoint.h"
#include "cwPenStroke.h"
#include "cwScale.h"
#include "cwSketch.h"
#include "cwSketchPainter.h"
#include "cwSketchScrapRasterizer.h"

namespace {

double expectedPpm(double paperScale)
{
    return cwSketchPainter::pixelsPerMeterFromPaperScale(
        paperScale, cwSketchScrapRasterizer::kTargetDPI);
}

// Same default cwSketch ships with: 1:250 paper scale from
// cwSketch::cwSketch() via scaleDenominator = 250.
constexpr double kDefault1to250 = 1.0 / 250.0;

// Any alpha above the threshold counts as a painted pixel — rules out
// background transparent pixels while tolerating anti-aliased edges.
bool pixelPainted(const QImage &img, int x, int y)
{
    if(x < 0 || y < 0 || x >= img.width() || y >= img.height()) {
        return false;
    }
    const QColor c = img.pixelColor(x, y);
    return c.alpha() > 32;
}

// Horizontal stroke at world y = cy that runs from (x0, cy) to (x1, cy),
// finalised so the painter path model picks it up.
void drawHorizontalStroke(cwSketch &sketch,
                          cwPenStroke::Kind kind,
                          double width,
                          const QColor &color,
                          double x0, double x1, double cy)
{
    const int row = sketch.beginStroke(kind, width, color);
    sketch.appendPoint(row, cwPenPoint(QPointF(x0, cy), 0.5));
    sketch.appendPoint(row, cwPenPoint(QPointF(x1, cy), 0.5));
    sketch.endStroke();
}

} // namespace

TEST_CASE("cwSketchScrapRasterizer image size matches paperScale × DPI × bbox",
          "[cwSketchScrapRasterizer]")
{
    cwSketch sketch;
    // Default mapScale is 1:250 (see cwSketch ctor).
    REQUIRE(sketch.mapScale()->scale() == kDefault1to250);

    drawHorizontalStroke(sketch, cwPenStroke::Wall, 2.0, Qt::red,
                         0.0, 10.0, 5.0);

    const QRectF bbox(0.0, 0.0, 10.0, 10.0);
    const QImage image = cwSketchScrapRasterizer::rasterize(&sketch, bbox);

    const double ppm = expectedPpm(kDefault1to250);
    // Rounding up via ceil(rawPixels), so the image is at most 1 px larger
    // per axis than the exact floating-point size.
    CHECK(image.width()  >= static_cast<int>(ppm * bbox.width())  - 1);
    CHECK(image.width()  <= static_cast<int>(ppm * bbox.width())  + 2);
    CHECK(image.height() >= static_cast<int>(ppm * bbox.height()) - 1);
    CHECK(image.height() <= static_cast<int>(ppm * bbox.height()) + 2);
}

TEST_CASE("cwSketchScrapRasterizer clamps to maxPixelDimension for huge bboxes",
          "[cwSketchScrapRasterizer]")
{
    cwSketch sketch;
    drawHorizontalStroke(sketch, cwPenStroke::Wall, 2.0, Qt::black,
                         0.0, 500.0, 250.0);

    // At 1:250 + 200 DPI (~31 px/m), a 400 m axis would want ~12.6k pixels —
    // well above the default 4096 cap.
    const QRectF bbox(0.0, 0.0, 400.0, 200.0);
    const int maxDim = 4096;
    const QImage image = cwSketchScrapRasterizer::rasterize(&sketch, bbox, maxDim);

    REQUIRE(!image.isNull());
    CHECK(image.width()  <= maxDim);
    CHECK(image.height() <= maxDim);
    CHECK(std::max(image.width(), image.height()) == maxDim);

    // Aspect ratio preserved — width/height on output should still match
    // the bbox aspect (within one pixel for ceil rounding).
    const double imgAspect  = double(image.width())  / double(image.height());
    const double bboxAspect = bbox.width() / bbox.height();
    CHECK(std::abs(imgAspect - bboxAspect) < 0.02);
}

TEST_CASE("cwSketchScrapRasterizer paints strokes inside the bbox and skips strokes outside",
          "[cwSketchScrapRasterizer]")
{
    cwSketch sketch;

    // Inside stroke: horizontal red bar at world y = 5, from x = 2 to 8.
    drawHorizontalStroke(sketch, cwPenStroke::Wall, 6.0, Qt::red,
                         2.0, 8.0, 5.0);

    // Outside stroke: far east, well clear of the bbox we'll rasterize.
    drawHorizontalStroke(sketch, cwPenStroke::Feature, 6.0, Qt::blue,
                         100.0, 110.0, 105.0);

    const QRectF bbox(0.0, 0.0, 10.0, 10.0);
    const QImage image = cwSketchScrapRasterizer::rasterize(&sketch, bbox);
    REQUIRE(!image.isNull());

    // A pixel near the middle of the image (world (~5, ~5)) should land on
    // the red stroke — any painted alpha there is enough to confirm the
    // stroke made it through.
    const int midX = image.width()  / 2;
    const int midY = image.height() / 2;
    CHECK(pixelPainted(image, midX, midY));

    // No blue anywhere: the outside-bbox stroke must not contribute.
    bool sawBlue = false;
    for(int y = 0; y < image.height() && !sawBlue; ++y) {
        for(int x = 0; x < image.width(); ++x) {
            const QColor c = image.pixelColor(x, y);
            if(c.alpha() > 32 && c.blue() > c.red() + 40 && c.blue() > c.green() + 40) {
                sawBlue = true;
                break;
            }
        }
    }
    CHECK_FALSE(sawBlue);
}

TEST_CASE("cwSketchScrapRasterizer returns a null image for degenerate input",
          "[cwSketchScrapRasterizer]")
{
    cwSketch sketch;

    CHECK(cwSketchScrapRasterizer::rasterize(nullptr, QRectF(0, 0, 1, 1)).isNull());
    CHECK(cwSketchScrapRasterizer::rasterize(&sketch, QRectF()).isNull());
    CHECK(cwSketchScrapRasterizer::rasterize(&sketch,
                                             QRectF(0, 0, 1, 1),
                                             /*maxPixelDimension*/ 0).isNull());
}

TEST_CASE("cwSketchScrapRasterizer image size scales with non-default paper scale",
          "[cwSketchScrapRasterizer]")
{
    cwSketch sketch;
    // A 1:100 scale is 2.5× denser on paper than the 1:250 default —
    // same bbox should produce ~2.5× the pixels per axis.
    sketch.mapScale()->scaleDenominator()->setValue(100.0);

    const QRectF bbox(0.0, 0.0, 3.0, 4.0);
    const QImage image = cwSketchScrapRasterizer::rasterize(&sketch, bbox);

    REQUIRE(!image.isNull());
    const double expected = expectedPpm(1.0 / 100.0) * bbox.width();
    CHECK(image.width() >= static_cast<int>(expected) - 1);
    CHECK(image.width() <= static_cast<int>(expected) + 2);
}
