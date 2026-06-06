/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSketchCanvasRenderer.h"
#include "cwSketchCanvas.h"
#include "cwSketch.h"
#include "cwScale.h"
#include "cwSketchDrawCanvas.h"
#include "cwSketchPainter.h"
#include "cwSketchPathSource.h"
#include "cwCenterlineSketchPainterModel.h"
#include "cwInfiniteGridModel.h"
#include "cwFixedGridModel.h"
#include "cwGridTextModel.h"
#include "cwSketchScrapRasterizer.h"

//Qt includes
#include <QCanvasPainter>
#include <QCanvasPainterItem>
#include <QColor>
#include <QFont>
#include <QPainterPath>

// Render-thread-owned path list backing cwSketchPainter::paint. Populated in
// synchronize() while the GUI thread is blocked; read in paint() without
// touching the live GUI-thread path sources.
class cwSketchCanvasRendererSnapshotModel final : public cwSketchPathSource
{
public:
    QList<Path> entries;

    QList<Path> paths() const override {
        return entries;
    }
};

namespace {

void snapshotPaths(const cwSketchPathSource *source,
                   cwSketchCanvasRendererSnapshotModel *target)
{
    target->entries.clear();
    if (source == nullptr) {
        return;
    }
    const QList<cwSketchPathSource::Path> paths = source->paths();
    target->entries.reserve(paths.size());
    for (const auto &path : paths) {
        if (path.painterPath.isEmpty()) {
            continue;
        }
        target->entries.append(path);
    }
}

} // namespace

cwSketchCanvasRenderer::cwSketchCanvasRenderer()
    : m_snapshot(new cwSketchCanvasRendererSnapshotModel()),
      m_minorGridSnapshot(new cwSketchCanvasRendererSnapshotModel()),
      m_majorGridSnapshot(new cwSketchCanvasRendererSnapshotModel()),
      m_linePlotSnapshot(new cwSketchCanvasRendererSnapshotModel())
{
}

cwSketchCanvasRenderer::~cwSketchCanvasRenderer()
{
    delete m_snapshot;
    delete m_minorGridSnapshot;
    delete m_majorGridSnapshot;
    delete m_linePlotSnapshot;
}

void cwSketchCanvasRenderer::synchronize(QCanvasPainterItem *item)
{
    auto *canvas = qobject_cast<cwSketchCanvas *>(item);
    if (canvas == nullptr) {
        m_snapshot->entries.clear();
        m_minorGridSnapshot->entries.clear();
        m_majorGridSnapshot->entries.clear();
        m_linePlotSnapshot->entries.clear();
        m_minorTextSnapshot.clear();
        m_majorTextSnapshot.clear();
        m_linePlotTextSnapshot.clear();
        m_debugSnapshot.clear();
        m_rejectedSnapshot.clear();
        return;
    }

    const QPointF pan = canvas->pan();
    const QMatrix4x4 mapMatrix = canvas->mapMatrix();
    const double userZoom = canvas->zoom();
    m_mapScale = mapMatrix(0, 0);
    m_userZoom = userZoom;
    // Dimensionless paper:world ratio (e.g. 1/250). Falls back to 1:250 if
    // the sketch isn't attached yet — matches the linePlot model's default.
    auto *sketch = canvas->sketch();
    auto *sketchScale = sketch ? sketch->mapScale() : nullptr;
    m_mapScaleRatio = (sketchScale && sketchScale->scale() > 0.0)
                          ? sketchScale->scale()
                          : cwSketchPainter::LinePlotReferenceMapScaleRatio;
    m_worldToItem = mapMatrix.toTransform()
                    * QTransform().scale(userZoom, userZoom)
                    * QTransform().translate(pan.x(), pan.y());

    if (userZoom > 0.0) {
        const QRectF itemRect(0.0, 0.0, canvas->width(), canvas->height());
        m_worldViewport = m_worldToItem.inverted().mapRect(itemRect);
    } else {
        m_worldViewport = QRectF();
    }

    snapshotPaths(canvas->pathModel(), m_snapshot);
    auto *linePlot = canvas->linePlotModel();
    snapshotPaths(linePlot, m_linePlotSnapshot);
    m_linePlotTextSnapshot = linePlot
                                 ? linePlot->textRows()
                                 : QVector<cwGridTextModel::TextRow>();

    auto *grid = canvas->grid();
    snapshotPaths(grid ? grid->minorGridModel() : nullptr, m_minorGridSnapshot);
    snapshotPaths(grid ? grid->majorGridModel() : nullptr, m_majorGridSnapshot);
    m_minorTextSnapshot = (grid && grid->minorTextModel())
                              ? grid->minorTextModel()->rows()
                              : QVector<cwGridTextModel::TextRow>();
    m_majorTextSnapshot = (grid && grid->majorTextModel())
                              ? grid->majorTextModel()->rows()
                              : QVector<cwGridTextModel::TextRow>();

    auto *scrapManager = canvas->scrapManager();
    m_debugSnapshot = (canvas->debugOverlayVisible() && scrapManager && sketch)
                          ? scrapManager->sketchDebugEntries(sketch)
                          : QVector<cwScrapManager::SketchScrapDebugEntry>();
    m_rejectedSnapshot = (canvas->debugOverlayVisible() && scrapManager && sketch)
                             ? scrapManager->sketchRejectedStrokes(sketch)
                             : QVector<cwSketchScrapRejectedStroke>();
}

void cwSketchCanvasRenderer::paint(QCanvasPainter *painter)
{
    const float w = width();
    const float h = height();
    if (w <= 0.0f || h <= 0.0f) {
        return;
    }

    painter->clearRect(0.0f, 0.0f, w, h);
    painter->setRenderHint(QCanvasPainter::RenderHint::Antialiasing);

    const bool hasStrokes  = !m_snapshot->entries.isEmpty();
    const bool hasLinePlot = !m_linePlotSnapshot->entries.isEmpty()
                          || !m_linePlotTextSnapshot.isEmpty();
    const bool hasGrid     = !m_minorGridSnapshot->entries.isEmpty()
                          || !m_majorGridSnapshot->entries.isEmpty()
                          || !m_minorTextSnapshot.isEmpty()
                          || !m_majorTextSnapshot.isEmpty();

    if (!hasStrokes && !hasGrid && !hasLinePlot
        && m_debugSnapshot.isEmpty() && m_rejectedSnapshot.isEmpty()) {
        return;
    }

    painter->save();
    cwSketchDrawCanvas draw(painter);

    if (hasStrokes || hasGrid || hasLinePlot) {
        cwSketchPainter::PaintContext ctx;
        ctx.viewport       = m_worldViewport;
        ctx.worldToItem    = m_worldToItem;
        ctx.mapScale       = m_mapScale;
        ctx.strokes        = m_snapshot;
        ctx.gridMinor.paths = m_minorGridSnapshot;
        ctx.gridMinor.text  = &m_minorTextSnapshot;
        ctx.gridMajor.paths = m_majorGridSnapshot;
        ctx.gridMajor.text  = &m_majorTextSnapshot;
        ctx.linePlot.paths  = m_linePlotSnapshot;
        ctx.linePlot.text   = &m_linePlotTextSnapshot;

        ctx.linePlotTextScale =
            (m_mapScaleRatio / cwSketchPainter::LinePlotReferenceMapScaleRatio) * m_userZoom;

        // Strokes render at a fixed world thickness (independent of screen
        // DPI) to match the scrap rasterizer and outline outset.
        ctx.strokePenScale = cwSketchPainter::paperStrokePenScale(
            m_mapScaleRatio, cwSketchScrapRasterizer::kTargetDPI);

        cwSketchPainter::paint(&draw, ctx);
    }

    if (!m_debugSnapshot.isEmpty()) {
        const QColor outlineColor(0, 200, 230, 230);
        const QColor stationFill(255, 80, 80, 230);
        const QColor stationLabel(20, 20, 20, 255);
        const double markerRadius = 5.0;
        const QFont labelFont(QStringLiteral("Helvetica"), 10);

        draw.save();
        draw.setTransform(QTransform());
        draw.setStrokePen(outlineColor, 2.0, Qt::FlatCap, Qt::MiterJoin);
        draw.setFillBrush(stationFill);

        for (const auto &entry : m_debugSnapshot) {
            if (entry.tripLocalPolygon.size() >= 2) {
                QPainterPath path;
                path.moveTo(m_worldToItem.map(entry.tripLocalPolygon.first()));
                for (int i = 1; i < entry.tripLocalPolygon.size(); ++i) {
                    path.lineTo(m_worldToItem.map(entry.tripLocalPolygon.at(i)));
                }
                path.closeSubpath();
                draw.strokePath(path);
            }

            for (const auto &station : entry.stations) {
                const QPointF itemPos = m_worldToItem.map(station.tripLocalPos);
                QPainterPath dot;
                dot.addEllipse(itemPos, markerRadius, markerRadius);
                draw.fillPath(dot);
                draw.drawText(itemPos + QPointF(markerRadius + 2.0, -markerRadius),
                              station.name, labelFont, stationLabel);
            }
        }
        draw.restore();
    }

    if (!m_rejectedSnapshot.isEmpty()) {
        const QColor rejectColor(220, 60, 60, 230);
        const QColor labelColor(180, 20, 20, 255);
        const QFont  labelFont(QStringLiteral("Helvetica"), 10);

        draw.save();
        draw.setTransform(QTransform());
        draw.setStrokePen(rejectColor, 2.0, Qt::FlatCap, Qt::MiterJoin);

        for (const auto &rejected : m_rejectedSnapshot) {
            const QPolygonF &poly = rejected.tripLocalPolyline;
            if (poly.size() < 2) {
                continue;
            }
            QPainterPath path;
            QPointF centroid(0.0, 0.0);
            path.moveTo(m_worldToItem.map(poly.first()));
            centroid += poly.first();
            for (int i = 1; i < poly.size(); ++i) {
                const QPointF mapped = m_worldToItem.map(poly.at(i));
                path.lineTo(mapped);
                centroid += poly.at(i);
            }
            draw.strokePath(path);

            centroid /= poly.size();
            draw.drawText(m_worldToItem.map(centroid), rejected.reason,
                          labelFont, labelColor);
        }
        draw.restore();
    }

    painter->restore();
}
