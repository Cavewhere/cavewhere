/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch includes
#include <catch2/catch_test_macros.hpp>

// Our includes
#include "cwCompressedImageItem.h"
#include "cwGraphicsImageItem.h"

// Qt includes
#include <QImage>
#include <QPainter>

namespace {

// A tile-like image: transparent except for some ink, in the format export
// grabs arrive in.
QImage makeInkedTile()
{
    QImage image(256, 128, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(Qt::black, 3.0));
    painter.setBrush(QColor(80, 120, 40, 180));
    painter.drawEllipse(QPointF(64.0, 64.0), 40.0, 25.0);
    painter.drawLine(QPointF(10.0, 100.0), QPointF(240.0, 20.0));
    return image;
}

// Paints a QGraphicsItem directly (both item classes ignore the style option
// and widget) onto a fresh canvas the size of the source image.
template <typename Item>
QImage paintItem(Item& item, const QSize& size)
{
    QImage canvas(size, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::white);
    QPainter painter(&canvas);
    item.paint(&painter, nullptr, nullptr);
    return canvas;
}

} // namespace

TEST_CASE("cwCompressedImageItem paints identically to cwGraphicsImageItem",
          "[cwCompressedImageItem]")
{
    const QImage tile = makeInkedTile();

    cwGraphicsImageItem rawItem;
    rawItem.setImage(tile);

    cwCompressedImageItem compressedItem;
    compressedItem.setImage(tile);

    CHECK(compressedItem.boundingRect() == rawItem.boundingRect());
    CHECK(compressedItem.compressedImage().size() == tile.size());
    CHECK(compressedItem.compressedImage().format() == tile.format());
    CHECK(compressedItem.compressedImage().rawSizeInBytes() == tile.sizeInBytes());
    // The whole point: a mostly transparent tile must shrink.
    CHECK(compressedItem.compressedImage().compressedData().size() < tile.sizeInBytes());
    // And the compress → decompress round trip must be bit-exact.
    CHECK(compressedItem.compressedImage().decompress() == tile);

    const QImage fromRaw = paintItem(rawItem, tile.size());
    const QImage fromCompressed = paintItem(compressedItem, tile.size());
    CHECK(fromCompressed == fromRaw);
}

TEST_CASE("cwCompressedImageItem paints at raw pixel size regardless of source devicePixelRatio",
          "[cwCompressedImageItem]")
{
    // The export path pairs this item's raw-pixel paint with an item-level
    // setScale(1/dpr) (cwCaptureViewport), so paint() must IGNORE the source
    // image's devicePixelRatio — honoring it would shrink every export tile
    // twice. Pin that contract: a dpr=2 source still covers its full pixel
    // extent.
    QImage tile = makeInkedTile();
    tile.setDevicePixelRatio(2.0);

    cwCompressedImageItem item;
    item.setImage(tile);

    CHECK(item.boundingRect() == QRectF(QPointF(), QSizeF(tile.size())));

    const QImage canvas = paintItem(item, tile.size());
    // The diagonal line's ink reaches x=240 in raw pixels. A dpr-honoring
    // paint would draw the source at half size (128x64), leaving the right
    // half of the canvas untouched white.
    bool inkOnRightHalf = false;
    for(int y = 0; y < canvas.height() && !inkOnRightHalf; y++) {
        const QRgb* row = reinterpret_cast<const QRgb*>(canvas.constScanLine(y));
        for(int x = canvas.width() / 2 + 1; x < canvas.width(); x++) {
            if(row[x] != qRgb(255, 255, 255)) {
                inkOnRightHalf = true;
                break;
            }
        }
    }
    CHECK(inkOnRightHalf);
}

TEST_CASE("cwCompressedImageItem survives repeated paints and null images",
          "[cwCompressedImageItem]")
{
    cwCompressedImageItem item;

    // Null image: nothing stored, painting is a no-op on the canvas.
    item.setImage(QImage());
    CHECK(item.boundingRect().isEmpty());
    QImage canvas(QSize(32, 32), QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::white);
    {
        QPainter painter(&canvas);
        item.paint(&painter, nullptr, nullptr);
    }
    QImage untouched(canvas.size(), canvas.format());
    untouched.fill(Qt::white);
    CHECK(canvas == untouched);

    // Decompress-in-paint must be repeatable — the stored blob is not
    // consumed by painting.
    const QImage tile = makeInkedTile();
    item.setImage(tile);
    const QImage first = paintItem(item, tile.size());
    const QImage second = paintItem(item, tile.size());
    CHECK(first == second);
}
