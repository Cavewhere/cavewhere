/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCHSCRAPRASTERIZER_H
#define CWSKETCHSCRAPRASTERIZER_H

//Qt includes
#include <QImage>
#include <QRectF>

//Our includes
#include "CaveWhereLibExport.h"

class cwSketch;

/**
  Rasterizes the strokes inside a sketch-derived scrap's bounding box into a
  QImage that the triangulator feeds into cwRenderTexturedItems. The image is
  sized by pixels-per-cave-meter = sketch paper-scale × kTargetDPI (clamped at
  maxPixelDimension). Output is a transparent ARGB32 QImage with every stroke
  whose bounding box intersects the scrap bounding box painted at its native
  pen width / colour — the bbox + QPainter clip rect skip strokes that live
  entirely outside the scrap.
  */
class CAVEWHERE_LIB_EXPORT cwSketchScrapRasterizer
{
public:
    // Target resolution in dots-per-paper-inch. Converted to pixels per
    // cave-meter using the sketch's paper scale inside rasterize(). A
    // compile-time constant; a future iteration may expose it if needed.
    static constexpr double kTargetDPI = 200.0;

    static QImage rasterize(const cwSketch *sketch,
                            const QRectF   &tripLocalBoundingBox,
                            int             maxPixelDimension = 4096);
};

#endif // CWSKETCHSCRAPRASTERIZER_H
