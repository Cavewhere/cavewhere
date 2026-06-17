/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwDecoratedStrokePathSource.h"
#include "cwSketch.h"
#include "cwSketchStrokeGeometry.h"
#include "cwPenStroke.h"
#include "cwGlyphSubPath.h"
#include "cwLineBrush.h"

//Qt includes
#include <QGuiApplication>
#include <QStyleHints>

//Std includes
#include <algorithm>
#include <optional>

namespace {
// Pressure-ribbon tuning for the in-progress stroke preview (Decision C); the
// decorated finished render no longer uses these.
constexpr double kPreviewMaxHalfWidth = 3.0;
constexpr double kPreviewMinHalfWidth = 0.25;
constexpr double kPreviewWidthScale   = 1.5;
constexpr int    kPreviewEndPointTessellation = 5;
} // namespace

cwDecoratedStrokePathSource::cwDecoratedStrokePathSource(QObject *parent)
    : QObject(parent),
      m_layout(&m_cache)
{
    if (QStyleHints *hints = QGuiApplication::styleHints()) {
        m_darkMode = hints->colorScheme() == Qt::ColorScheme::Dark;
        connect(hints, &QStyleHints::colorSchemeChanged,
                this, &cwDecoratedStrokePathSource::onColorSchemeChanged);
    }
}

const cwSketch *cwDecoratedStrokePathSource::sketch() const
{
    return m_sketch;
}

void cwDecoratedStrokePathSource::setSketch(const cwSketch *sketch)
{
    if (m_sketch == sketch) {
        return;
    }

    if (m_sketch) {
        disconnect(m_sketch, nullptr, this, nullptr);
    }

    m_sketch = sketch;
    m_finished.clear();
    m_activePath = Path{};
    m_previousActiveStroke = -1;

    if (m_sketch) {
        connect(m_sketch, &cwSketch::strokeInserted, this,
                &cwDecoratedStrokePathSource::onStrokeInserted);
        connect(m_sketch, &cwSketch::strokeRemoved, this,
                &cwDecoratedStrokePathSource::onStrokeRemoved);
        connect(m_sketch, &cwSketch::strokeChanged, this,
                &cwDecoratedStrokePathSource::onStrokeChanged);
        connect(m_sketch, &cwSketch::strokesReset, this,
                &cwDecoratedStrokePathSource::onStrokesReset);

        rebuildAllFinished();
        buildActivePath();
    }

    emit pathsChanged();
}

void cwDecoratedStrokePathSource::setActiveStrokeIndex(int index)
{
    if (m_activeStrokeIndex == index) {
        return;
    }
    m_activeStrokeIndex = index;
    emit activeStrokeIndexChanged();
    onActiveStrokeIndexChanged();
}

void cwDecoratedStrokePathSource::setActiveStrokeColor(const QColor &color)
{
    if (m_activeStrokeColor == color) {
        return;
    }
    m_activeStrokeColor = color;
    emit activeStrokeColorChanged();
    buildActivePath();
    emit pathsChanged();
}

void cwDecoratedStrokePathSource::setSnapshot(const cwPaletteSnapshot &snapshot)
{
    m_snapshot = snapshot;
    m_cache.setSnapshot(snapshot);
    scheduleRebuild();
}

void cwDecoratedStrokePathSource::setMapScaleRatio(double ratio)
{
    // ratio <= 0 is ignored: keep the last good scale rather than divide by zero
    // in the paper-mm -> world-m bake.
    if (ratio <= 0.0 || qFuzzyCompare(m_mapScaleRatio, ratio)) {
        return;
    }
    m_mapScaleRatio = ratio;
    scheduleRebuild();
}

QList<cwSketchPathSource::Path> cwDecoratedStrokePathSource::paths() const
{
    // Finished batches emit in (z, emission) order — stable so equal-z batches
    // keep their fold order (= layer index within a stroke). The painter also
    // z-sorts, but ordering here keeps the source's own contract testable.
    QList<Path> finished;
    finished.reserve(m_finished.size());
    for (const Batch &batch : m_finished) {
        finished.append(batch.path);
    }
    std::stable_sort(finished.begin(), finished.end(),
                     [](const Path &a, const Path &b) { return a.z < b.z; });

    QList<Path> result;
    result.reserve(finished.size() + 1);
    result.append(m_activePath);
    result.append(finished);
    return result;
}

int cwDecoratedStrokePathSource::strokeCount() const
{
    return m_sketch ? static_cast<int>(m_sketch->strokes().size()) : 0;
}

QVector<QPointF> cwDecoratedStrokePathSource::strokeCenterline(int row) const
{
    QVector<QPointF> world;
    if (!m_sketch || row < 0 || row >= m_sketch->strokes().size()) {
        return world;
    }
    const QVector<cwPenPoint> &points = m_sketch->strokes().at(row).points;
    world.reserve(points.size());
    for (const cwPenPoint &point : points) {
        world.append(point.position);
    }
    return world;
}

void cwDecoratedStrokePathSource::onStrokeInserted(int row)
{
    if (row == m_activeStrokeIndex) {
        buildActivePath();
    } else {
        // Incremental: only this stroke's layout runs and folds into batches.
        foldFinishedStroke(row);
    }
    emit pathsChanged();
}

void cwDecoratedStrokePathSource::onStrokeRemoved(int row)
{
    Q_UNUSED(row);
    // Batches don't track which source strokes contributed to them, so rebuild
    // from the (already-updated) stroke list. Synchronous: strokeRemoved fires
    // after the removal lands.
    scheduleRebuild();
}

void cwDecoratedStrokePathSource::onStrokeChanged(int row)
{
    // Only the active stroke mutates via strokeChanged (live pen input); finished
    // strokes are immutable, so a finished batch never needs an in-place update.
    if (row == m_activeStrokeIndex) {
        buildActivePath();
        emit pathsChanged();
    }
}

void cwDecoratedStrokePathSource::onStrokesReset()
{
    scheduleRebuild();
}

void cwDecoratedStrokePathSource::onActiveStrokeIndexChanged()
{
    buildActivePath();
    // The stroke that was active is now committed/finished — decorate it.
    if (m_previousActiveStroke >= 0
        && m_previousActiveStroke < strokeCount()
        && m_previousActiveStroke != m_activeStrokeIndex) {
        foldFinishedStroke(m_previousActiveStroke);
    }
    m_previousActiveStroke = m_activeStrokeIndex;
    emit pathsChanged();
}

void cwDecoratedStrokePathSource::onColorSchemeChanged(Qt::ColorScheme scheme)
{
    const bool dark = scheme == Qt::ColorScheme::Dark;
    if (dark == m_darkMode) {
        return;
    }
    m_darkMode = dark;
    // Fast path (Decision D): flip each batch's resolved color from its source
    // pair; no geometry rebuild, no re-layout.
    reResolveColors();
    emit pathsChanged();
}

void cwDecoratedStrokePathSource::scheduleRebuild()
{
    rebuildAllFinished();
    buildActivePath();
    emit pathsChanged();
}

void cwDecoratedStrokePathSource::rebuildAllFinished()
{
    m_finished.clear();
    if (!m_sketch) {
        return;
    }
    for (int row = 0; row < strokeCount(); ++row) {
        if (row == m_activeStrokeIndex) {
            continue;
        }
        foldFinishedStroke(row);
    }
}

void cwDecoratedStrokePathSource::foldFinishedStroke(int row)
{
    if (!m_sketch || row < 0 || row >= strokeCount()) {
        return;
    }

    const std::optional<cwLineBrush> brush =
        m_snapshot.findBrush(m_sketch->strokes().at(row).brushName);
    if (!brush.has_value()) {
        return; // unknown brush -> no ink
    }

    const QVector<QPointF> world = strokeCenterline(row);
    if (world.size() < 2) {
        return;
    }

    const cwBrushDecorationGeometry geometry =
        m_layout.layout(*brush, world, m_mapScaleRatio);

    const double z = brush->zOrder;

    // layout() emits one geometry layer per brush decoration, in order, so the
    // indices line up — the traced regions take their layer's authored pen
    // style; stamps have no per-glyph cap/join in the data model, so the Path
    // defaults stand.
    const int layerCount = (std::min)(geometry.layers.size(), brush->decorations.size());
    for (int i = 0; i < layerCount; ++i) {
        const cwBrushDecorationGeometry::Layer &layer = geometry.layers.at(i);
        const cwDecorationLayer &decoration = brush->decorations.at(i);

        for (const cwGlyphSubPath &traced : layer.traced) {
            foldSubPath(traced, decoration.lineCap, decoration.lineJoin, z);
        }
        for (const cwResolvedStamp &stamp : layer.stamps) {
            for (const cwGlyphSubPath &sub : stamp.subPaths) {
                foldSubPath(sub, Qt::RoundCap, Qt::RoundJoin, z);
            }
        }
    }
}

void cwDecoratedStrokePathSource::foldSubPath(const cwGlyphSubPath &sub,
                                              Qt::PenCapStyle cap,
                                              Qt::PenJoinStyle join,
                                              double z)
{
    // Fill pass for a Polygon carrying a fill color: a fill (strokeWidth <= 0)
    // item, drawn before the pen so the outline lands on top.
    if (sub.kind == cwGlyphSubPath::Polygon && sub.fillColorLight.isValid()) {
        Batch &fill = batchFor(sub.fillColorLight, sub.fillColorDark,
                               /*width*/ -1.0, /*widthInWorldMetres*/ false,
                               cap, join, z);
        fill.path.painterPath.addPath(sub.path);
    }

    // Pen pass: only when the sub-path actually has an outline (valid color and
    // positive width). The pure-fill degenerate case (penWidthMm == 0) has none.
    if (sub.penColorLight.isValid() && sub.penWidthMm > 0.0) {
        const double worldWidth =
            sub.penWidthMm * cwGlyphTessellationCache::paperMmToWorldM(m_mapScaleRatio);
        Batch &pen = batchFor(sub.penColorLight, sub.penColorDark,
                              worldWidth, /*widthInWorldMetres*/ true,
                              cap, join, z);
        pen.path.painterPath.addPath(sub.path);
    }
}

cwDecoratedStrokePathSource::Batch &cwDecoratedStrokePathSource::batchFor(
    const QColor &srcLight, const QColor &srcDark,
    double width, bool widthInWorldMetres,
    Qt::PenCapStyle cap, Qt::PenJoinStyle join, double z)
{
    auto it = std::find_if(m_finished.begin(), m_finished.end(),
                           [&](const Batch &b) {
        return b.srcLight == srcLight
            && b.srcDark == srcDark
            && b.path.strokeWidth == width
            && b.path.widthInWorldMetres == widthInWorldMetres
            && b.path.cap == cap
            && b.path.join == join
            && b.path.z == z;
    });
    if (it != m_finished.end()) {
        return *it;
    }

    Batch fresh;
    fresh.srcLight                 = srcLight;
    fresh.srcDark                  = srcDark;
    fresh.path.strokeColor         = resolveColor(srcLight, srcDark);
    fresh.path.strokeWidth         = width;
    fresh.path.widthInWorldMetres  = widthInWorldMetres;
    fresh.path.cap                 = cap;
    fresh.path.join                = join;
    fresh.path.z                   = z;
    m_finished.append(fresh);
    return m_finished.last();
}

void cwDecoratedStrokePathSource::reResolveColors()
{
    for (Batch &batch : m_finished) {
        batch.path.strokeColor = resolveColor(batch.srcLight, batch.srcDark);
    }
}

QColor cwDecoratedStrokePathSource::resolveColor(const QColor &light, const QColor &dark) const
{
    return m_darkMode ? dark : light;
}

void cwDecoratedStrokePathSource::buildActivePath()
{
    m_activePath = Path{};
    m_activePath.strokeColor = m_activeStrokeColor;
    m_activePath.strokeWidth = kSketchStrokeRenderWidth; // screen pixels (Decision C)

    if (m_activeStrokeIndex < 0 || !m_sketch
        || m_activeStrokeIndex >= strokeCount()) {
        return;
    }
    buildPreviewGeometry(m_activePath.painterPath, m_activeStrokeIndex);
}

void cwDecoratedStrokePathSource::buildPreviewGeometry(QPainterPath &out, int row) const
{
    if (!m_sketch || row < 0 || row >= m_sketch->strokes().size()) {
        return;
    }
    const QVector<cwPenPoint> &points = m_sketch->strokes().at(row).points;
    if (points.size() < 2) {
        return;
    }

    cwSketchStrokeGeometry::Params params;
    params.maxHalfWidth         = kPreviewMaxHalfWidth;
    params.minHalfWidth         = kPreviewMinHalfWidth;
    params.widthScale           = kPreviewWidthScale;
    params.endPointTessellation = kPreviewEndPointTessellation;

    cwSketchStrokeGeometry::buildPath(out, points, kSketchStrokeRenderWidth, params);
}
