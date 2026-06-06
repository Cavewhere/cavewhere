/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSketchPainterPathModel.h"
#include "cwSketch.h"
#include "cwSketchStrokeGeometry.h"
#include "cwPenStroke.h"
#include "cwSymbologyPaletteData.h"
#include "cwSymbologyPaletteSeed.h"

//Qt includes
#include <QPolygonF>
#include <cmath>

cwSketchPainterPathModel::cwSketchPainterPathModel(QObject *parent)
    : QObject(parent),
      m_snapshot(cwSymbologyPaletteSeed::create().snapshot())
{
    connect(this, &cwSketchPainterPathModel::activeStrokeIndexChanged,
            this, &cwSketchPainterPathModel::onActiveStrokeIndexChanged);
}

void cwSketchPainterPathModel::setWallStrokeColor(const QColor &color)
{
    if (m_wallStrokeColor == color) {
        return;
    }
    m_wallStrokeColor = color;
    emit wallStrokeColorChanged();
    scheduleColorRebuild();
}

void cwSketchPainterPathModel::setNonWallStrokeColor(const QColor &color)
{
    if (m_nonWallStrokeColor == color) {
        return;
    }
    m_nonWallStrokeColor = color;
    emit nonWallStrokeColorChanged();
    scheduleColorRebuild();
}

// Coalesce wall+nonWall writes that arrive in the same event-loop turn
// (a theme toggle fires both bindings) into a single rebuild pass.
// addOrUpdateFinishPath batches by (width, rgba), so a color change
// shifts batch membership — finished paths must be torn down and rebuilt.
void cwSketchPainterPathModel::scheduleColorRebuild()
{
    if (m_colorRebuildPending) {
        return;
    }
    m_colorRebuildPending = true;
    QMetaObject::invokeMethod(this, [this]() {
        m_colorRebuildPending = false;
        rebuildAllFinished();
        updateActivePath();
    }, Qt::QueuedConnection);
}

const cwSketch *cwSketchPainterPathModel::sketch() const
{
    return m_sketch;
}

void cwSketchPainterPathModel::setSketch(const cwSketch *sketch)
{
    if (m_sketch == sketch) {
        return;
    }

    if (m_sketch) {
        disconnect(m_sketch, nullptr, this, nullptr);
    }

    m_sketch = sketch;
    m_activePath = Path{};
    m_finishedPaths.clear();
    m_previousActiveStroke = -1;

    if (m_sketch) {
        connect(m_sketch, &cwSketch::strokeInserted, this,
                &cwSketchPainterPathModel::onStrokeInserted);
        connect(m_sketch, &cwSketch::strokeRemoved, this,
                &cwSketchPainterPathModel::onStrokeRemoved);
        connect(m_sketch, &cwSketch::strokeChanged, this,
                &cwSketchPainterPathModel::onStrokeChanged);
        connect(m_sketch, &cwSketch::strokesReset, this,
                &cwSketchPainterPathModel::onStrokesReset);

        for (int row = 0; row < strokeCount(); ++row) {
            if (row == m_activeStrokeIndex) {
                continue;
            }
            mergeFinishPath(row);
        }
        updateActivePath();
    }

    emit pathsChanged();
}

int cwSketchPainterPathModel::strokeCount() const
{
    return m_sketch ? static_cast<int>(m_sketch->strokes().size()) : 0;
}

QVector<cwPenPoint> cwSketchPainterPathModel::strokePoints(int row) const
{
    if (!m_sketch || row < 0 || row >= m_sketch->strokes().size()) {
        return {};
    }
    return m_sketch->strokes().at(row).points;
}

double cwSketchPainterPathModel::strokeWidth(int row) const
{
    Q_UNUSED(row);
    return kSketchStrokeRenderWidth;
}

QColor cwSketchPainterPathModel::strokeColor(int row) const
{
    if (!m_sketch || row < 0 || row >= m_sketch->strokes().size()) {
        return m_wallStrokeColor;
    }
    const QString name = m_sketch->strokes().at(row).brushName;
    const bool wallClass = m_snapshot.producesScrapOutline(name);
    return wallClass ? m_wallStrokeColor : m_nonWallStrokeColor;
}

QList<cwSketchPathSource::Path> cwSketchPainterPathModel::paths() const
{
    QList<Path> result;
    result.reserve(m_finishedPaths.size() + 1);
    result.append(m_activePath);
    result.append(m_finishedPaths);
    return result;
}

void cwSketchPainterPathModel::onStrokeInserted(int row)
{
    if (row == m_activeStrokeIndex) {
        updateActivePath();
    } else {
        addOrUpdateFinishPath(row);
    }
}

void cwSketchPainterPathModel::onStrokeRemoved(int row)
{
    Q_UNUSED(row);
    // Batches don't track which source strokes contributed to them, so rebuild
    // from the (already-updated) stroke list. Synchronous: strokeRemoved fires
    // after the removal lands, unlike the old rowsAboutToBeRemoved signal.
    rebuildAllFinished();
}

void cwSketchPainterPathModel::onStrokeChanged(int row)
{
    // Only the active stroke mutates via strokeChanged (live pen input);
    // finished strokes are immutable. Updating a finished batch in-place would
    // re-append geometry and double-count.
    if (row == m_activeStrokeIndex) {
        updateActivePath();
    }
}

void cwSketchPainterPathModel::onStrokesReset()
{
    rebuildAllFinished();
    updateActivePath();
}

void cwSketchPainterPathModel::onActiveStrokeIndexChanged()
{
    updateActivePath();
    if (m_previousActiveStroke >= 0
        && m_previousActiveStroke < strokeCount()) {
        addOrUpdateFinishPath(m_previousActiveStroke);
    }
    m_previousActiveStroke = m_activeStrokeIndex;
}

void cwSketchPainterPathModel::updateActivePath()
{
    m_activePath.painterPath.clear();

    if (m_activeStrokeIndex < 0 || !m_sketch
        || m_activeStrokeIndex >= strokeCount()) {
        m_activePath.strokeWidth = 0.0;
        m_activePath.strokeColor = m_wallStrokeColor;
        emit pathsChanged();
        return;
    }

    const double w = strokeWidth(m_activeStrokeIndex);
    const QColor c = strokeColor(m_activeStrokeIndex);
    m_activePath.strokeWidth = w;
    m_activePath.strokeColor = c;
    buildStrokeGeometry(m_activePath.painterPath, m_activeStrokeIndex);

    emit pathsChanged();
}

void cwSketchPainterPathModel::rebuildAllFinished()
{
    m_finishedPaths.clear();
    if (m_sketch) {
        for (int row = 0; row < strokeCount(); ++row) {
            if (row == m_activeStrokeIndex) {
                continue;
            }
            mergeFinishPath(row);
        }
    }
    emit pathsChanged();
}

void cwSketchPainterPathModel::addOrUpdateFinishPath(int sourceRow)
{
    if (mergeFinishPath(sourceRow)) {
        emit pathsChanged();
    }
}

// Folds the stroke into its (width, rgba) batch without emitting, so loop
// callers (rebuildAllFinished, setSketch) can emit pathsChanged() once for the
// whole pass. Returns true when the finished set changed.
bool cwSketchPainterPathModel::mergeFinishPath(int sourceRow)
{
    if (!m_sketch || sourceRow < 0 || sourceRow >= strokeCount()) {
        return false;
    }

    const double width = strokeWidth(sourceRow);
    const QColor color = strokeColor(sourceRow);
    const QRgb   rgba  = color.rgba();

    auto it = std::find_if(m_finishedPaths.begin(), m_finishedPaths.end(),
                           [width, rgba](const Path &p) {
                               return p.strokeWidth == width
                                      && p.strokeColor.rgba() == rgba;
                           });
    if (it != m_finishedPaths.end()) {
        buildStrokeGeometry(it->painterPath, sourceRow);
        return true;
    }

    Path fresh;
    fresh.strokeWidth = width;
    fresh.strokeColor = color;
    buildStrokeGeometry(fresh.painterPath, sourceRow);
    m_finishedPaths.append(fresh);
    return true;
}

void cwSketchPainterPathModel::buildStrokeGeometry(QPainterPath &out, int sourceRow) const
{
    const QVector<cwPenPoint> points = strokePoints(sourceRow);
    if (points.size() < 2) {
        return;
    }

    cwSketchStrokeGeometry::Params params;
    params.maxHalfWidth         = m_maxHalfWidth;
    params.minHalfWidth         = m_minHalfWidth;
    params.widthScale           = m_widthScale;
    params.endPointTessellation = m_endPointTessellation;

    cwSketchStrokeGeometry::buildPath(out, points, strokeWidth(sourceRow), params);
}
