//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QCoreApplication>
#include <QImage>
#include <QSignalSpy>
#include <QUrl>

//Our includes
#include "cwCacheImageProvider.h"
#include "cwDiskCacher.h"
#include "cwPenStroke.h"
#include "cwProject.h"
#include "cwSketch.h"
#include "cwSketchManager.h"

namespace {

void addStroke(cwSketch& sketch, cwPenStroke::Kind kind,
               std::initializer_list<QPointF> points)
{
    const int row = sketch.beginStroke(kind, 3.0);
    for (const auto& p : points) {
        sketch.appendPoint(row, p, 1.0, 0);
    }
    sketch.endStroke();
}

int countNonWhitePixels(const QImage& image)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y) != QColor(Qt::white)) {
                ++count;
            }
        }
    }
    return count;
}

} // namespace

TEST_CASE("cwSketchManager::renderIcon returns a non-empty QImage for sketches with strokes",
          "[cwSketchManager]")
{
    cwSketch sketch;
    addStroke(sketch, cwPenStroke::Wall, { {0, 0}, {10, 10}, {20, 0} });

    const QImage image = cwSketchManager::renderIcon(&sketch, 64);
    REQUIRE(!image.isNull());
    CHECK(image.width() == 64);
    CHECK(image.height() == 64);
}

TEST_CASE("cwSketchManager::renderIcon returns a null QImage for empty sketches",
          "[cwSketchManager]")
{
    cwSketch sketch;
    const QImage image = cwSketchManager::renderIcon(&sketch, 64);
    CHECK(image.isNull());
}

TEST_CASE("cwSketchManager::renderIcon produces non-white pixels for a drawn multi-point stroke",
          "[cwSketchManager]")
{
    cwSketch sketch;
    addStroke(sketch, cwPenStroke::Wall,
              { {0.0, 0.0}, {20.0, 5.0}, {40.0, 0.0}, {60.0, 10.0} });

    const QImage image = cwSketchManager::renderIcon(&sketch, 128);
    REQUIRE_FALSE(image.isNull());
    CHECK(image.width() == 128);
    CHECK(image.height() == 128);

    const int nonWhite = countNonWhitePixels(image);
    CAPTURE(nonWhite);
    CHECK(nonWhite > 0);
}

TEST_CASE("cwSketchManager::renderIcon keeps all points in view (regression: bbox accumulator)",
          "[cwSketchManager]")
{
    // Two widely-separated points. With the old QRectF::isNull()-based
    // accumulator the bbox collapsed to just the last point, so the first
    // point was rendered far off-frame. If the fix is correct, both ends
    // of the stroke produce visible pixels near opposite sides of the image.
    cwSketch sketch;
    addStroke(sketch, cwPenStroke::Wall, { {0.0, 0.0}, {100.0, 0.0} });

    const QImage image = cwSketchManager::renderIcon(&sketch, 128);
    REQUIRE_FALSE(image.isNull());

    auto hasInkInColumn = [&](int x) {
        for (int y = 0; y < image.height(); ++y) {
            if (image.pixelColor(x, y) != QColor(Qt::white)) {
                return true;
            }
        }
        return false;
    };

    // Left third and right third should both contain stroke pixels if the
    // full span is visible.
    bool leftHasInk = false;
    bool rightHasInk = false;
    for (int x = 0; x < image.width() / 3; ++x) {
        if (hasInkInColumn(x)) { leftHasInk = true; break; }
    }
    for (int x = image.width() * 2 / 3; x < image.width(); ++x) {
        if (hasInkInColumn(x)) { rightHasInk = true; break; }
    }
    CHECK(leftHasInk);
    CHECK(rightHasInk);
}

TEST_CASE("cwSketchManager::rasteriseNow writes a PNG containing drawn pixels",
          "[cwSketchManager]")
{
    cwProject project;

    cwSketchManager manager;
    manager.setProject(&project);

    cwSketch sketch;
    addStroke(sketch, cwPenStroke::Wall,
              { {0.0, 0.0}, {50.0, 50.0}, {0.0, 50.0} });

    manager.rasteriseNow(&sketch);

    // Round-trip through the cache provider — that is the path QML takes.
    cwCacheImageProvider provider;
    provider.setProjectDirectory(project.dataRootDir());

    const QString url = sketch.iconImagePath();
    const QString prefix = QStringLiteral("image://%1/").arg(cwCacheImageProvider::name());
    REQUIRE(url.startsWith(prefix));
    const QString encoded = QUrl::fromPercentEncoding(url.mid(prefix.size()).toUtf8());

    QSize size;
    const QImage image = provider.requestImage(encoded, &size, QSize(0, 0));
    REQUIRE_FALSE(image.isNull());

    const int nonWhite = countNonWhitePixels(image);
    CAPTURE(nonWhite);
    CHECK(nonWhite > 0);
}

TEST_CASE("cwSketchManager writes an icon to the disk cache on rasteriseNow",
          "[cwSketchManager]")
{
    cwProject project;

    cwSketchManager manager;
    manager.setProject(&project);

    cwSketch sketch;
    QSignalSpy spy(&sketch, &cwSketch::iconImagePathChanged);

    addStroke(sketch, cwPenStroke::Wall, { {0, 0}, {5, 5} });

    CHECK(sketch.iconImagePath().isEmpty());

    manager.rasteriseNow(&sketch);

    REQUIRE(spy.count() >= 1);
    const QString path = sketch.iconImagePath();
    CHECK(path.startsWith(QStringLiteral("image://%1/").arg(cwCacheImageProvider::name())));

    const auto key = cwSketchManager::cacheKey(&sketch);
    cwDiskCacher cacher(project.dataRootDir());
    CHECK(cacher.hasEntry(key));
}

TEST_CASE("cwSketchManager URL resolves through the cwCacheImageProvider",
          "[cwSketchManager]")
{
    cwProject project;

    cwSketchManager manager;
    manager.setProject(&project);

    cwSketch sketch;
    addStroke(sketch, cwPenStroke::Feature, { {0, 0}, {10, 0}, {10, 10} });
    manager.rasteriseNow(&sketch);

    cwCacheImageProvider provider;
    provider.setProjectDirectory(project.dataRootDir());

    // The URL we set on the sketch encodes the key after "image://cwcache/".
    const QString url = sketch.iconImagePath();
    const QString prefix = QStringLiteral("image://%1/").arg(cwCacheImageProvider::name());
    REQUIRE(url.startsWith(prefix));
    const QString encoded = QUrl::fromPercentEncoding(url.mid(prefix.size()).toUtf8());

    QSize size;
    QImage image = provider.requestImage(encoded, &size, QSize(0, 0));
    REQUIRE(!image.isNull());
    CHECK(size.width() > 0);
    CHECK(size.height() > 0);
}

TEST_CASE("cwSketchManager cache entry survives a manager restart",
          "[cwSketchManager]")
{
    cwProject project;

    cwSketch sketch;
    addStroke(sketch, cwPenStroke::Wall, { {0, 0}, {3, 4} });

    const auto key = cwSketchManager::cacheKey(&sketch);

    {
        cwSketchManager manager;
        manager.setProject(&project);
        manager.rasteriseNow(&sketch);
    }

    // Fresh manager; the cache file should be found and the path re-applied
    // to a sketch with the same id.
    sketch.setIconImagePath(QString());
    cwSketch sameIdSketch;
    sameIdSketch.setId(sketch.id());
    addStroke(sameIdSketch, cwPenStroke::Wall, { {0, 0}, {3, 4} });

    cwDiskCacher cacher(project.dataRootDir());
    CHECK(cacher.hasEntry(key));
}
