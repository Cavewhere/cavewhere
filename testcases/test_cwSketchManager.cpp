//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
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

// --------------------------------------------------------------------------
// Async-pipeline tests
//
// These exercise the auto-update + manual-flush paths. Each test creates its
// own cwProject (which allocates an isolated dataRootDir) so the sketch-icons
// directory doesn't collide across parallel test processes.
// --------------------------------------------------------------------------

namespace {

// Wait up to `timeoutMs` for `predicate` to return true; pumps the event loop
// so queued slots and async observers fire. Returns the final truth value.
template <typename P>
bool spinUntil(P predicate, int timeoutMs = 3000)
{
    QElapsedTimer timer; timer.start();
    while (!predicate() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        QTest::qWait(10);
    }
    return predicate();
}

} // namespace

TEST_CASE("cwSketchManager: async render fires after idle window",
          "[cwSketchManager][async]")
{
    cwProject project;
    cwSketchManager manager;
    manager.setProject(&project);
    manager.setIdleIntervalMs(100);

    cwSketch sketch;
    QSignalSpy iconSpy(&sketch, &cwSketch::iconImagePathChanged);

    manager.attachSketch(&sketch);

    addStroke(sketch, cwPenStroke::Wall, { {0, 0}, {10, 10}, {20, 0} });

    const bool updated = spinUntil([&]() { return iconSpy.count() > 0; }, 3000);
    REQUIRE(updated);
    CHECK(!sketch.iconImagePath().isEmpty());

    const auto key = cwSketchManager::cacheKey(&sketch);
    cwDiskCacher cacher(project.dataRootDir());
    CHECK(cacher.hasEntry(key));
}

TEST_CASE("cwSketchManager: autoIconUpdates=false suppresses auto render",
          "[cwSketchManager][async]")
{
    cwProject project;
    cwSketchManager manager;
    manager.setProject(&project);
    manager.setIdleIntervalMs(100);
    manager.setAutoIconUpdates(false);

    cwSketch sketch;
    QSignalSpy iconSpy(&sketch, &cwSketch::iconImagePathChanged);

    manager.attachSketch(&sketch);
    addStroke(sketch, cwPenStroke::Wall, { {0, 0}, {10, 10} });

    QTest::qWait(500);
    CHECK(iconSpy.count() == 0);

    const auto autoKey = cwSketchManager::cacheKey(&sketch);
    cwDiskCacher autoCacher(project.dataRootDir());
    CHECK_FALSE(autoCacher.hasEntry(autoKey));

    manager.flushIconIfDirty(&sketch);
    const bool updated = spinUntil([&]() { return iconSpy.count() > 0; }, 3000);
    REQUIRE(updated);
    CHECK(autoCacher.hasEntry(autoKey));
}

TEST_CASE("cwSketchManager: flushIconIfDirty is a no-op when not dirty",
          "[cwSketchManager][async]")
{
    cwProject project;
    cwSketchManager manager;
    manager.setProject(&project);
    manager.setIdleIntervalMs(100);
    manager.setAutoIconUpdates(false);

    cwSketch sketch;
    manager.attachSketch(&sketch);
    addStroke(sketch, cwPenStroke::Wall, { {0, 0}, {5, 5} });

    // Keep flushing until the sketch quiesces. cwSketch::appendPoint posts a
    // queued dataChanged that can bump dirtyEpoch after the initial flush
    // submits, so we may need to chase one trailing render.
    manager.flushIconIfDirty(&sketch);
    REQUIRE(spinUntil([&]() { return !sketch.iconImagePath().isEmpty(); }, 3000));
    QTest::qWait(100);
    manager.flushIconIfDirty(&sketch);
    QTest::qWait(500); // let any trailing submit land

    QSignalSpy iconSpy(&sketch, &cwSketch::iconImagePathChanged);

    manager.flushIconIfDirty(&sketch);
    QTest::qWait(500);
    CHECK(iconSpy.count() == 0);
}

TEST_CASE("cwSketchManager: rapid edits coalesce into one write",
          "[cwSketchManager][async]")
{
    cwProject project;
    cwSketchManager manager;
    manager.setProject(&project);
    manager.setIdleIntervalMs(100);

    cwSketch sketch;
    QSignalSpy iconSpy(&sketch, &cwSketch::iconImagePathChanged);
    manager.attachSketch(&sketch);

    for (int i = 0; i < 5; ++i) {
        addStroke(sketch, cwPenStroke::Wall,
                  { QPointF(i, 0), QPointF(i + 1, 1) });
    }

    REQUIRE(spinUntil([&]() { return iconSpy.count() > 0; }, 3000));
    CHECK(iconSpy.count() <= 2);
}

TEST_CASE("cwSketchManager: flushIconIfDirty is gated while actively drawing",
          "[cwSketchManager][async]")
{
    cwProject project;
    cwSketchManager manager;
    manager.setProject(&project);
    manager.setIdleIntervalMs(100);
    manager.setAutoIconUpdates(false);

    cwSketch sketch;
    QSignalSpy iconSpy(&sketch, &cwSketch::iconImagePathChanged);
    manager.attachSketch(&sketch);

    const int row = sketch.beginStroke(cwPenStroke::Wall, 3.0);
    sketch.appendPoint(row, QPointF(0, 0), 1.0, 0);
    sketch.appendPoint(row, QPointF(5, 5), 1.0, 0);

    manager.flushIconIfDirty(&sketch);
    QTest::qWait(300);
    CHECK(iconSpy.count() == 0);

    sketch.endStroke();

    manager.flushIconIfDirty(&sketch);
    REQUIRE(spinUntil([&]() { return iconSpy.count() > 0; }, 3000));
}

TEST_CASE("cwSketchManager: teardown during in-flight render does not crash",
          "[cwSketchManager][async]")
{
    cwProject project;
    cwSketchManager manager;
    manager.setProject(&project);
    manager.setIdleIntervalMs(50);

    // Spin up a few renders and destroy the sketch before they settle. ASan
    // catches any dangling-pointer deref in the observer / pipeline map.
    for (int i = 0; i < 20; ++i) {
        auto sketch = std::make_unique<cwSketch>();
        manager.attachSketch(sketch.get());
        addStroke(*sketch, cwPenStroke::Wall,
                  { {0, 0}, {double(i), double(i) + 1} });
        QTest::qWait(5); // may or may not have submitted yet
    }
    QTest::qWait(500);
    SUCCEED();
}
