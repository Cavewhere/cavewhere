/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSketchScrapRasterizer.h"
#include "cwScale.h"
#include "cwSketch.h"
#include "cwSketchDrawQPainter.h"
#include "cwSketchPainter.h"
#include "cwSketchPainterPathModel.h"

//Qt includes
#include <QPainter>
#include <QTransform>

#include <algorithm>
#include <cmath>

QImage cwSketchScrapRasterizer::rasterize(const cwSketch *sketch,
                                          const QRectF   &tripLocalBoundingBox,
                                          int             maxPixelDimension)
{
    if(sketch == nullptr || sketch->mapScale() == nullptr
        || tripLocalBoundingBox.isEmpty() || maxPixelDimension <= 0) {
        return QImage();
    }

    const double paperScale = sketch->mapScale()->scale();
    if(paperScale <= 0.0) {
        return QImage();
    }

    const double basePpm =
        cwSketchPainter::pixelsPerMeterFromPaperScale(paperScale, kTargetDPI);
    if(basePpm <= 0.0) {
        return QImage();
    }

    const double rawW = basePpm * tripLocalBoundingBox.width();
    const double rawH = basePpm * tripLocalBoundingBox.height();
    const double rawMax = std::max(rawW, rawH);
    const double renderScale =
        rawMax > static_cast<double>(maxPixelDimension)
            ? static_cast<double>(maxPixelDimension) / rawMax
            : 1.0;

    const double ppm = basePpm * renderScale;
    const int imageW = std::max(1, static_cast<int>(std::ceil(rawW * renderScale)));
    const int imageH = std::max(1, static_cast<int>(std::ceil(rawH * renderScale)));

    QImage image(imageW, imageH, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    // World meters → image pixels with Y-flip so world-north (max Y) lands
    // at the image's top row. Matches the thumbnail path in cwSketchManager,
    // just sized to the scrap's bounding box instead of a square icon.
    QTransform worldToItem;
    worldToItem.translate(-tripLocalBoundingBox.left()  * ppm,
                           tripLocalBoundingBox.bottom() * ppm);
    worldToItem.scale(ppm, -ppm);

    cwSketchPainterPathModel pathModel;
    pathModel.setSketch(sketch);

    cwSketchDrawQPainter draw(&painter);

    cwSketchPainter::PaintContext ctx;
    ctx.viewport    = tripLocalBoundingBox;
    ctx.worldToItem = worldToItem;
    // mapScale = pixels-per-meter so cwSketchPainter's pen-width cancellation
    // leaves stored widths at their stored (pixel) thickness in the output
    // image — same convention the on-screen thumbnail rasterizer uses. The
    // paper-bound exporter cancels paperScale instead because it targets a
    // device-coordinate transform; ours is pixel-coordinate.
    ctx.mapScale    = ppm;
    ctx.strokes     = &pathModel;
    cwSketchPainter::paint(&draw, ctx);

    painter.end();
    return image;
}
