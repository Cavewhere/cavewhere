/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCaptureTileRun.h"
#include "cwCompressedImageItem.h"
#include "cwGraphicsImageItem.h"
#include "cwSignalSpy.h"
#include "TestHelper.h"

//Qt includes
#include <QGraphicsItemGroup>
#include <QImage>
#include <QColor>
#include <QPoint>
#include <QPromise>

namespace {

constexpr int kTileSizePx = 16;
constexpr int kFinishedTimeoutMs = 10000;
const QColor kInkColorA(20, 80, 40);
const QColor kInkColorB(200, 10, 10);
constexpr QPoint kInkPixelA(3, 4);
constexpr QPoint kInkPixelB(10, 12);

QImage makeInkedTile()
{
    QImage image(kTileSizePx, kTileSizePx, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    image.setPixelColor(kInkPixelA, kInkColorA);
    image.setPixelColor(kInkPixelB, kInkColorB);
    return image;
}

QImage makeBlankTile()
{
    QImage image(kTileSizePx, kTileSizePx, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    return image;
}

QFuture<QImage> finishedFuture(const QImage& image)
{
    QPromise<QImage> promise;
    promise.start();
    promise.addResult(image);
    promise.finish();
    return promise.future();
}

QFuture<QImage> emptyFinishedFuture()
{
    QPromise<QImage> promise;
    promise.start();
    promise.finish();
    return promise.future();
}

QFuture<QImage> canceledFuture()
{
    QPromise<QImage> promise;
    promise.start();
    promise.future().cancel();
    promise.finish();
    return promise.future();
}

// finished() is delivered through queued AsyncFuture continuations (and, for
// export tiles, a worker-thread compression hop), so the test has to pump the
// event loop until it lands.
bool waitForFinished(cwSignalSpy& finishedSpy, int timeoutMs = kFinishedTimeoutMs)
{
    return finishedSpy.size() >= 1 || finishedSpy.wait(timeoutMs);
}

} // namespace

TEST_CASE("cwCaptureTileRun streams export tiles into compressed items and drops blank ones",
          "[cwCaptureTileRun]")
{
    QGraphicsItemGroup parent;
    cwCaptureTileRun run;
    cwSignalSpy finishedSpy(&run, &cwCaptureTileRun::finished);

    const QImage inked = makeInkedTile();
    const QList<cwCaptureTileRun::Tile> tiles = {
        {QPointF(0.0, 0.0), finishedFuture(inked)},
        {QPointF(kTileSizePx, 0.0), finishedFuture(makeBlankTile())},
        {QPointF(0.0, kTileSizePx), finishedFuture(inked)},
    };

    run.start(tiles, &parent, false, cwFutureManagerToken());

    REQUIRE(waitForFinished(finishedSpy));
    CHECK(finishedSpy.size() == 1);
    CHECK(run.isCanceled() == false);

    // The blank tile places nothing; the inked tiles come back compressed,
    // positioned, and bit-exact.
    const QList<QGraphicsItem*> children = parent.childItems();
    REQUIRE(children.size() == 2);
    QList<QPointF> positions;
    for(QGraphicsItem* child : children) {
        auto* tileItem = dynamic_cast<cwCompressedImageItem*>(child);
        REQUIRE(tileItem != nullptr);
        CHECK(tileItem->compressedImage().decompress() == inked);
        positions.append(tileItem->pos());
    }
    CHECK(positions.contains(QPointF(0.0, 0.0)));
    CHECK(positions.contains(QPointF(0.0, kTileSizePx)));
}

TEST_CASE("cwCaptureTileRun keeps preview tiles as raw image items",
          "[cwCaptureTileRun]")
{
    QGraphicsItemGroup parent;
    cwCaptureTileRun run;
    cwSignalSpy finishedSpy(&run, &cwCaptureTileRun::finished);

    const QImage inked = makeInkedTile();
    const QList<cwCaptureTileRun::Tile> tiles = {
        {QPointF(0.0, 0.0), finishedFuture(inked)},
        // Previews keep blank tiles too: the preview bounding box derives
        // from its tile items.
        {QPointF(kTileSizePx, 0.0), finishedFuture(makeBlankTile())},
    };

    run.start(tiles, &parent, true, cwFutureManagerToken());

    REQUIRE(waitForFinished(finishedSpy));
    CHECK(finishedSpy.size() == 1);

    const QList<QGraphicsItem*> children = parent.childItems();
    REQUIRE(children.size() == 2);
    for(QGraphicsItem* child : children) {
        CHECK(dynamic_cast<cwGraphicsImageItem*>(child) != nullptr);
        CHECK(dynamic_cast<cwCompressedImageItem*>(child) == nullptr);
    }
}

TEST_CASE("cwCaptureTileRun finishes once even when tiles fail or cancel",
          "[cwCaptureTileRun]")
{
    QGraphicsItemGroup parent;
    cwCaptureTileRun run;
    cwSignalSpy finishedSpy(&run, &cwCaptureTileRun::finished);

    // One good tile, one that settled without producing an image, and one
    // whose render was canceled: the failed tiles degrade to missing tiles
    // and must still count, or the run would never finish.
    const QList<cwCaptureTileRun::Tile> tiles = {
        {QPointF(0.0, 0.0), finishedFuture(makeInkedTile())},
        {QPointF(kTileSizePx, 0.0), emptyFinishedFuture()},
        {QPointF(0.0, kTileSizePx), canceledFuture()},
    };

    run.start(tiles, &parent, false, cwFutureManagerToken());

    REQUIRE(waitForFinished(finishedSpy));
    CHECK(finishedSpy.size() == 1);
    CHECK(run.isCanceled() == false);
    CHECK(parent.childItems().size() == 1);
}

TEST_CASE("cwCaptureTileRun cancel drops placement but still finishes",
          "[cwCaptureTileRun]")
{
    QGraphicsItemGroup parent;
    cwCaptureTileRun run;
    cwSignalSpy finishedSpy(&run, &cwCaptureTileRun::finished);

    // The futures are finished but their settle handlers haven't run yet
    // (delivery is queued), so the cancel below beats every handler — the
    // canceled run must drop all pixels without placing items and still
    // count through to finished().
    const QImage inked = makeInkedTile();
    const QList<cwCaptureTileRun::Tile> tiles = {
        {QPointF(0.0, 0.0), finishedFuture(inked)},
        {QPointF(kTileSizePx, 0.0), finishedFuture(inked)},
    };

    run.start(tiles, &parent, false, cwFutureManagerToken());
    run.cancel();

    REQUIRE(waitForFinished(finishedSpy));
    CHECK(finishedSpy.size() == 1);
    CHECK(run.isCanceled() == true);
    CHECK(parent.childItems().isEmpty());
}

TEST_CASE("cwCaptureTileRun cancel mid-run deletes the tiles it already placed",
          "[cwCaptureTileRun]")
{
    QGraphicsItemGroup parent;
    cwCaptureTileRun run;
    cwSignalSpy finishedSpy(&run, &cwCaptureTileRun::finished);

    // The second tile's render is still in flight when the cancel arrives,
    // so the first tile has already streamed into parent.
    QPromise<QImage> pendingTile;
    pendingTile.start();

    const QImage inked = makeInkedTile();
    const QList<cwCaptureTileRun::Tile> tiles = {
        {QPointF(0.0, 0.0), finishedFuture(inked)},
        {QPointF(kTileSizePx, 0.0), pendingTile.future()},
    };

    run.start(tiles, &parent, true, cwFutureManagerToken());

    REQUIRE(waitUntil([&parent]() { return parent.childItems().size() == 1; },
                      kFinishedTimeoutMs));
    CHECK(finishedSpy.isEmpty());

    run.cancel();
    CHECK(parent.childItems().isEmpty());

    pendingTile.addResult(inked);
    pendingTile.finish();

    REQUIRE(waitForFinished(finishedSpy));
    CHECK(finishedSpy.size() == 1);
    CHECK(run.isCanceled() == true);
    CHECK(parent.childItems().isEmpty());
}
