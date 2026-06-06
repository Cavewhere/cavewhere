/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSketchPainterPathModel.h"
#include "cwSketchStrokeGeometry.h"
#include "cwPenStroke.h"
#include "cwSymbologyPaletteData.h"
#include "cwSymbologyPaletteSeed.h"

//Qt includes
#include <QPolygonF>
#include <cmath>

namespace {
constexpr char kPointsRoleName[]    = "points";
constexpr char kBrushNameRoleName[] = "brushName";
} // namespace

cwSketchPainterPathModel::cwSketchPainterPathModel(QObject *parent)
    : cwAbstractSketchPainterPathModel(parent),
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

int cwSketchPainterPathModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_finishedPaths.size() + m_finishLineIndexOffset;
}

QAbstractItemModel *cwSketchPainterPathModel::strokeModel() const
{
    return m_strokeModel;
}

void cwSketchPainterPathModel::setStrokeModel(QAbstractItemModel *model)
{
    if (m_strokeModel == model) {
        return;
    }

    beginResetModel();
    if (m_strokeModel) {
        disconnect(m_strokeModel, nullptr, this, nullptr);
    }

    m_strokeModel = model;
    m_activePath = Path{};
    m_finishedPaths.clear();
    m_previousActiveStroke = -1;

    if (m_strokeModel) {
        resolveRoles();

        connect(m_strokeModel, &QAbstractItemModel::rowsInserted, this,
                &cwSketchPainterPathModel::onSourceRowsInserted);
        connect(m_strokeModel, &QAbstractItemModel::rowsAboutToBeRemoved, this,
                &cwSketchPainterPathModel::onSourceRowsAboutToBeRemoved);
        connect(m_strokeModel, &QAbstractItemModel::dataChanged, this,
                &cwSketchPainterPathModel::onSourceDataChanged);
        connect(m_strokeModel, &QAbstractItemModel::modelReset, this,
                &cwSketchPainterPathModel::onSourceModelReset);

        for (int row = 0; row < m_strokeModel->rowCount(); ++row) {
            if (row == m_activeStrokeIndex) {
                continue;
            }
            addOrUpdateFinishPath(row);
        }
        updateActivePath();
    }

    endResetModel();
    emit strokeModelChanged();
}

void cwSketchPainterPathModel::resolveRoles()
{
    const auto names = m_strokeModel->roleNames();
    m_pointsRole    = names.key(kPointsRoleName,    -1);
    m_brushNameRole = names.key(kBrushNameRoleName, -1);
    if (m_pointsRole < 0 || m_brushNameRole < 0) {
        qWarning("cwSketchPainterPathModel: source model is missing required "
                 "role names (points/brushName)");
    }
}

QVector<cwPenPoint> cwSketchPainterPathModel::strokePoints(int row) const
{
    if (!m_strokeModel || m_pointsRole < 0) {
        return {};
    }
    const QModelIndex idx = m_strokeModel->index(row, 0);
    return idx.data(m_pointsRole).value<QVector<cwPenPoint>>();
}

double cwSketchPainterPathModel::strokeWidth(int row) const
{
    Q_UNUSED(row);
    return kSketchStrokeRenderWidth;
}

QColor cwSketchPainterPathModel::strokeColor(int row) const
{
    if (!m_strokeModel || m_brushNameRole < 0) {
        return m_wallStrokeColor;
    }
    const QString name = m_strokeModel->index(row, 0).data(m_brushNameRole).toString();
    const bool wallClass = m_snapshot.producesScrapOutline(name);
    return wallClass ? m_wallStrokeColor : m_nonWallStrokeColor;
}

cwAbstractSketchPainterPathModel::Path
cwSketchPainterPathModel::path(const QModelIndex &index) const
{
    if (index.row() == 0) {
        return m_activePath;
    }
    return m_finishedPaths.at(index.row() - m_finishLineIndexOffset);
}

void cwSketchPainterPathModel::onSourceRowsInserted(const QModelIndex &parent,
                                                    int first, int last)
{
    if (parent.isValid()) {
        return;
    }
    for (int row = first; row <= last; ++row) {
        if (row == m_activeStrokeIndex) {
            updateActivePath();
        } else {
            addOrUpdateFinishPath(row);
        }
    }
}

void cwSketchPainterPathModel::onSourceRowsAboutToBeRemoved(const QModelIndex &parent,
                                                            int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    // Defer the rebuild: batches don't track which source rows contributed to
    // them, so the rebuild has to re-read the source model *after* the remove
    // lands. Iteration 3 will introduce per-batch row tracking for live edits.
    QMetaObject::invokeMethod(this, [this]{ rebuildAllFinished(); }, Qt::QueuedConnection);
}

void cwSketchPainterPathModel::onSourceDataChanged(const QModelIndex &tl,
                                                    const QModelIndex &br,
                                                    const QList<int> &roles)
{
    // Iteration 1 contract: only the active stroke mutates via dataChanged;
    // finished strokes are immutable. Updating a finished batch in-place would
    // re-append geometry and double-count.
    const bool touchesPoints = roles.isEmpty() || roles.contains(m_pointsRole);
    if (!touchesPoints) {
        return;
    }
    if (m_activeStrokeIndex >= tl.row() && m_activeStrokeIndex <= br.row()) {
        updateActivePath();
    }
}

void cwSketchPainterPathModel::onSourceModelReset()
{
    rebuildAllFinished();
    updateActivePath();
}

void cwSketchPainterPathModel::onActiveStrokeIndexChanged()
{
    updateActivePath();
    if (m_previousActiveStroke >= 0 && m_strokeModel
        && m_previousActiveStroke < m_strokeModel->rowCount()) {
        addOrUpdateFinishPath(m_previousActiveStroke);
    }
    m_previousActiveStroke = m_activeStrokeIndex;
}

void cwSketchPainterPathModel::updateActivePath()
{
    m_activePath.painterPath.clear();

    if (m_activeStrokeIndex < 0 || !m_strokeModel
        || m_activeStrokeIndex >= m_strokeModel->rowCount()) {
        m_activePath.strokeWidth = 0.0;
        m_activePath.strokeColor = m_wallStrokeColor;
        const QModelIndex rowIdx = index(0);
        emit dataChanged(rowIdx, rowIdx,
                         { PainterPathRole, StrokeWidthRole, StrokeColorRole });
        return;
    }

    const double w = strokeWidth(m_activeStrokeIndex);
    const QColor c = strokeColor(m_activeStrokeIndex);
    m_activePath.strokeWidth = w;
    m_activePath.strokeColor = c;
    buildStrokeGeometry(m_activePath.painterPath, m_activeStrokeIndex);

    const QModelIndex rowIdx = index(0);
    emit dataChanged(rowIdx, rowIdx,
                     { PainterPathRole, StrokeWidthRole, StrokeColorRole });
}

void cwSketchPainterPathModel::rebuildAllFinished()
{
    clearFinished();
    if (!m_strokeModel) {
        return;
    }
    for (int row = 0; row < m_strokeModel->rowCount(); ++row) {
        if (row == m_activeStrokeIndex) {
            continue;
        }
        addOrUpdateFinishPath(row);
    }
}

void cwSketchPainterPathModel::clearFinished()
{
    if (m_finishedPaths.isEmpty()) {
        return;
    }
    const int first = m_finishLineIndexOffset;
    const int last  = m_finishedPaths.size() - 1 + m_finishLineIndexOffset;
    beginRemoveRows(QModelIndex(), first, last);
    m_finishedPaths.clear();
    endRemoveRows();
}

void cwSketchPainterPathModel::addOrUpdateFinishPath(int sourceRow)
{
    if (!m_strokeModel || sourceRow < 0 || sourceRow >= m_strokeModel->rowCount()) {
        return;
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
        const int rowIdx = std::distance(m_finishedPaths.begin(), it)
                           + m_finishLineIndexOffset;
        const QModelIndex modelIdx = index(rowIdx);
        emit dataChanged(modelIdx, modelIdx,
                         { PainterPathRole, StrokeWidthRole, StrokeColorRole });
        return;
    }

    const int newRow = m_finishedPaths.size() + m_finishLineIndexOffset;
    beginInsertRows(QModelIndex(), newRow, newRow);
    Path fresh;
    fresh.strokeWidth = width;
    fresh.strokeColor = color;
    buildStrokeGeometry(fresh.painterPath, sourceRow);
    m_finishedPaths.append(fresh);
    endInsertRows();
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
