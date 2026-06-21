/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSketchCanvas.h"
#include "cwSketch.h"
#include "cwScale.h"
#include "cwSketchCanvasRenderer.h"
#include "cwSketchManager.h"
#include "cwScrapManager.h"
#include "cwDecoratedStrokePathSource.h"
#include "cwPaletteSnapshotSource.h"
#include "cwInfiniteGridModel.h"
#include "cwFixedGridModel.h"
#include "cwGridTextModel.h"
#include "cwCenterlineSketchPainterModel.h"
#include "cwStation.h"
#include "cwSurvey2DGeometryArtifact.h"
#include "cwSurvey2DGeometry.h"
#include "cwSymbologyPaletteSeed.h"
#include "cwTrip.h"
#include "Monad/Result.h"
#include "asyncfuture.h"

//Qt includes
#include <QAbstractItemModel>
#include <QDebug>
#include <QLoggingCategory>
#include <QTransform>

Q_LOGGING_CATEGORY(lcSketchCanvas, "cw.sketch.canvas")

cwSketchCanvas::cwSketchCanvas(QQuickItem *parent)
    : QCanvasPainterItem(parent),
      m_currentBrushName(cwSymbologyPaletteSeed::wallBrushName()),
      m_pathModel(new cwDecoratedStrokePathSource(this)),
      m_linePlotModel(new cwCenterlineSketchPainterModel(this)),
      m_linePlotGeometry(new cwSurvey2DGeometryArtifact(this))
{
    setFillColor(Qt::transparent);
    setAlphaBlending(true);
    connectPathModelSignals();

    m_linePlotGeometry->setName(QStringLiteral("SketchCanvas Line Plot"));
    m_linePlotModel->setSurvey2DGeometry(m_linePlotGeometry);
    connectModelForUpdate(m_linePlotModel);
}

cwSketchCanvas::~cwSketchCanvas()
{
    releaseLinePlotAcquisition();
}

cwSketch *cwSketchCanvas::sketch() const
{
    return m_sketch;
}

void cwSketchCanvas::setSketch(cwSketch *sketch)
{
    if (m_sketch == sketch) {
        return;
    }

    releaseLinePlotAcquisition();

    if (m_sketch) {
        disconnect(m_sketch, nullptr, this, nullptr);
    }

    m_sketch = sketch;

    if (m_mapScaleConnection) {
        QObject::disconnect(m_mapScaleConnection);
        m_mapScaleConnection = {};
    }

    if (m_sketch != nullptr) {
        m_pathModel->setSketch(m_sketch);
        refreshPathSnapshot();
        // Re-skin the render path whenever the sketch re-resolves its palette
        // (e.g. the region's defaultPaletteId changes). The `this` context lets
        // the blanket disconnect above tear this down on the next setSketch().
        connect(m_sketch, &cwSketch::paletteSnapshotChanged, this, [this]() {
            refreshPathSnapshot();
        });
        if (cwScale *scale = m_sketch->mapScale()) {
            m_pathModel->setMapScaleRatio(scale->scale());
            m_mapScaleConnection =
                connect(scale, &cwScale::scaleChanged, m_pathModel, [this, scale]() {
                    m_pathModel->setMapScaleRatio(scale->scale());
                });
        }
        m_linePlotModel->setMapScale(m_sketch->mapScale());
        connect(m_sketch, &cwSketch::strokesReset,
                this, [this]() { update(); });
        connect(m_sketch, &QObject::destroyed,
                this, [this]() { setSketch(nullptr); });

        if (m_viewStateDebugChangedConnection) {
            QObject::disconnect(m_viewStateDebugChangedConnection);
            m_viewStateDebugChangedConnection = {};
        }
        if (auto* viewState = m_sketch->viewState()) {
            m_viewStateDebugChangedConnection =
                connect(viewState, &cwSketchViewState::debugOverlayVisibleChanged,
                        this, [this]() {
                            emit debugOverlayVisibleChanged();
                            update();
                        });
        }

        acquireLinePlotForSketch(m_sketch);

        m_anchorStationChangedConnection =
            connect(m_sketch, &cwSketch::anchorStationChanged,
                    this, [this]() { refreshLinePlotFromManager(); });
    } else {
        m_pathModel->setSketch(nullptr);
        m_linePlotModel->setMapScale(nullptr);
    }

    emit sketchChanged();
    update();
}

void cwSketchCanvas::setCurrentBrushName(const QString &name)
{
    if (m_currentBrushName == name) {
        return;
    }
    m_currentBrushName = name;
    emit currentBrushNameChanged();
}

void cwSketchCanvas::setZoom(double zoom)
{
    if (qFuzzyCompare(m_zoom, zoom)) {
        return;
    }
    m_zoom = zoom;
    updateGridView();
    emit zoomChanged();
    update();
}

void cwSketchCanvas::setPan(QPointF pan)
{
    if (m_pan == pan) {
        return;
    }
    m_pan = pan;
    updateGridView();
    emit panChanged();
    update();
}

int cwSketchCanvas::activeStrokeIndex() const
{
    return m_pathModel->activeStrokeIndex();
}

void cwSketchCanvas::setActiveStrokeIndex(int index)
{
    if (m_pathModel->activeStrokeIndex() == index) {
        return;
    }
    m_pathModel->setActiveStrokeIndex(index);
    emit activeStrokeIndexChanged();
    update();
}

void cwSketchCanvas::setMapMatrix(const QMatrix4x4 &matrix)
{
    if (m_mapMatrix == matrix) {
        return;
    }
    m_mapMatrix = matrix;
    updateGridView();
    emit mapMatrixChanged();
    update();
}

void cwSketchCanvas::setGrid(cwInfiniteGridModel *grid)
{
    if (m_grid == grid) {
        return;
    }

    disconnectGridModels(m_grid);
    m_grid = grid;
    connectGridModels(m_grid);
    updateGridView();

    emit gridChanged();
    update();
}

cwPaletteSnapshot cwSketchCanvas::currentSnapshot() const
{
    if (m_snapshotSource != nullptr) {
        return m_snapshotSource->snapshot();
    }
    return m_sketch != nullptr ? m_sketch->paletteSnapshot() : cwPaletteSnapshot();
}

void cwSketchCanvas::refreshPathSnapshot()
{
    m_pathModel->setSnapshot(currentSnapshot());
}

cwPaletteSnapshotSource *cwSketchCanvas::snapshotSource() const
{
    return m_snapshotSource;
}

void cwSketchCanvas::setSnapshotSource(cwPaletteSnapshotSource *source)
{
    if (m_snapshotSource == source) {
        return;
    }

    if (m_snapshotSource != nullptr) {
        disconnect(m_snapshotSource, nullptr, this, nullptr);
    }

    m_snapshotSource = source;

    if (m_snapshotSource != nullptr) {
        connect(m_snapshotSource, &cwPaletteSnapshotSource::snapshotChanged,
                this, [this]() { refreshPathSnapshot(); });
        // If the source is destroyed while installed, m_snapshotSource auto-nulls
        // (QPointer); re-push so we fall back to the sketch's resolved palette.
        connect(m_snapshotSource, &QObject::destroyed,
                this, [this]() { refreshPathSnapshot(); });
    }

    refreshPathSnapshot();
}

QCanvasPainterItemRenderer *cwSketchCanvas::createItemRenderer() const
{
    return new cwSketchCanvasRenderer();
}

void cwSketchCanvas::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QCanvasPainterItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        updateGridView();
    }
}

void cwSketchCanvas::connectPathModelSignals()
{
    connect(m_pathModel, &cwDecoratedStrokePathSource::pathsChanged,
            this, [this]() { update(); });
}

void cwSketchCanvas::connectModelForUpdate(QAbstractItemModel *model)
{
    if (model == nullptr) {
        return;
    }
    auto requestUpdate = [this]() { update(); };
    connect(model, &QAbstractItemModel::dataChanged,   this, requestUpdate);
    connect(model, &QAbstractItemModel::rowsInserted,  this, requestUpdate);
    connect(model, &QAbstractItemModel::rowsRemoved,   this, requestUpdate);
    connect(model, &QAbstractItemModel::modelReset,    this, requestUpdate);
    connect(model, &QAbstractItemModel::layoutChanged, this, requestUpdate);
}

void cwSketchCanvas::connectModelForUpdate(cwFixedGridModel *model)
{
    if (model == nullptr) {
        return;
    }
    connect(model, &cwFixedGridModel::pathsChanged, this, [this]() { update(); });
}

void cwSketchCanvas::connectModelForUpdate(cwCenterlineSketchPainterModel *model)
{
    if (model == nullptr) {
        return;
    }
    connect(model, &cwCenterlineSketchPainterModel::pathsChanged, this, [this]() { update(); });
}

void cwSketchCanvas::disconnectGridModels(cwInfiniteGridModel *grid)
{
    if (grid == nullptr) {
        return;
    }
    disconnect(grid, nullptr, this, nullptr);
    if (auto *m = grid->majorGridModel()) { disconnect(m, nullptr, this, nullptr); }
    if (auto *m = grid->minorGridModel()) { disconnect(m, nullptr, this, nullptr); }
    if (auto *m = grid->majorTextModel()) { disconnect(m, nullptr, this, nullptr); }
    if (auto *m = grid->minorTextModel()) { disconnect(m, nullptr, this, nullptr); }
}

void cwSketchCanvas::connectGridModels(cwInfiniteGridModel *grid)
{
    if (grid == nullptr) {
        return;
    }
    connect(grid, &QObject::destroyed, this, [this]() { setGrid(nullptr); });
    connectModelForUpdate(grid->majorGridModel());
    connectModelForUpdate(grid->minorGridModel());
    connectModelForUpdate(grid->majorTextModel());
    connectModelForUpdate(grid->minorTextModel());
}

void cwSketchCanvas::updateGridView()
{
    if (m_grid == nullptr) {
        return;
    }

    // viewScale = 1/zoom is the pen-width / label-scale compensation factor
    // and drives the zoom-level log10 heuristic paired with mapMatrix scale.
    m_grid->setViewScale(m_zoom > 0.0 ? 1.0 / m_zoom : 1.0);

    if (m_zoom <= 0.0) {
        return;
    }

    const QTransform worldToItem =
        m_mapMatrix.toTransform()
        * QTransform().scale(m_zoom, m_zoom)
        * QTransform().translate(m_pan.x(), m_pan.y());

    bool invertible = false;
    const QTransform itemToWorld = worldToItem.inverted(&invertible);
    if (!invertible) {
        return;
    }

    const QRectF itemRect(0.0, 0.0, width(), height());
    const QRectF worldViewport = itemToWorld.mapRect(itemRect);
    const QPointF worldOrigin = itemToWorld.map(QPointF(0.0, 0.0));
    m_grid->setViewport(worldViewport);
    m_grid->setGridOrigin(worldOrigin);
}

void cwSketchCanvas::setScrapManager(cwScrapManager *manager)
{
    if (m_scrapManager == manager) {
        return;
    }
    if (m_scrapDebugChangedConnection) {
        QObject::disconnect(m_scrapDebugChangedConnection);
        m_scrapDebugChangedConnection = {};
    }
    m_scrapManager = manager;
    if (m_scrapManager) {
        m_scrapDebugChangedConnection =
            connect(m_scrapManager.data(), &cwScrapManager::sketchDiagnosticsChanged,
                    this, [this](cwSketch *changed) {
                        if (changed == m_sketch) {
                            update();
                        }
                    });
    }
    emit scrapManagerChanged();
    update();
}

bool cwSketchCanvas::debugOverlayVisible() const
{
    return m_sketch && m_sketch->viewState()
        && m_sketch->viewState()->debugOverlayVisible();
}

void cwSketchCanvas::setDebugOverlayVisible(bool visible)
{
    if (!m_sketch || !m_sketch->viewState()) {
        return;
    }
    m_sketch->viewState()->setDebugOverlayVisible(visible);
    // No need to emit or update() here — the viewState signal already
    // drives both via m_viewStateDebugChangedConnection.
}

void cwSketchCanvas::setSketchManager(cwSketchManager *manager)
{
    if (m_sketchManager == manager) {
        return;
    }
    releaseLinePlotAcquisition();
    m_sketchManager = manager;
    if (m_sketch != nullptr) {
        acquireLinePlotForSketch(m_sketch);
    }
    emit sketchManagerChanged();
}

void cwSketchCanvas::acquireLinePlotForSketch(cwSketch *sketch)
{
    if (sketch == nullptr || m_sketchManager.isNull()) {
        return;
    }
    cwTrip *trip = sketch->parentTrip();
    if (trip == nullptr) {
        return;
    }

    m_sketchManager->acquireLinePlot(trip);
    m_acquiredTrip = trip;

    m_linePlotUpdatedConnection = connect(
        m_sketchManager.data(), &cwSketchManager::linePlotUpdated,
        this, [this](cwTrip *updatedTrip) {
            if (updatedTrip == m_acquiredTrip.data()) {
                refreshLinePlotFromManager();
            }
        });

    refreshLinePlotFromManager();
}

void cwSketchCanvas::releaseLinePlotAcquisition()
{
    if (m_linePlotUpdatedConnection) {
        QObject::disconnect(m_linePlotUpdatedConnection);
        m_linePlotUpdatedConnection = {};
    }
    if (m_anchorStationChangedConnection) {
        QObject::disconnect(m_anchorStationChangedConnection);
        m_anchorStationChangedConnection = {};
    }
    if (!m_sketchManager.isNull() && !m_acquiredTrip.isNull()) {
        m_sketchManager->releaseLinePlot(m_acquiredTrip.data());
    }
    m_acquiredTrip.clear();
    clearLinePlotGeometry();
}

void cwSketchCanvas::clearLinePlotGeometry()
{
    m_linePlotGeometry->setGeometryResult(
        AsyncFuture::completed(Monad::Result<cwSurvey2DGeometry>(cwSurvey2DGeometry())));
}

namespace {

QList<cwTripComponentSummary> componentSummaries(const QList<cwTripLinePlotTask::TripComponent> &components)
{
    QList<cwTripComponentSummary> list;
    list.reserve(components.size());
    for (const auto &component : components) {
        cwTripComponentSummary summary;
        summary.anchor       = component.canonicalAnchor;
        summary.stations     = component.stations();
        summary.stationCount = summary.stations.size();
        list.append(std::move(summary));
    }
    return list;
}

int indexOfComponentContainingAnchor(
    const QList<cwTripLinePlotTask::TripComponent> &components,
    const QString &anchor)
{
    if (anchor.isEmpty()) {
        return -1;
    }
    for (int i = 0; i < components.size(); ++i) {
        if (components.at(i).positions.hasPosition(anchor)) {
            return i;
        }
    }
    return -1;
}

cwSurvey2DGeometry projectPlan(const cwTripLinePlotTask::TripComponent &component,
                               const QString &anchorStation)
{
    cwSurvey2DGeometry geometry;
    const QVector3D anchorPos = component.positions.position(anchorStation);

    const auto positionsMap = component.positions.positions();
    geometry.stations.reserve(positionsMap.size());
    QHash<QString, QPointF> name2D;
    name2D.reserve(positionsMap.size());

    for (auto it = positionsMap.constBegin(); it != positionsMap.constEnd(); ++it) {
        const QVector3D pos = it.value();
        const QPointF pos2D(pos.x() - anchorPos.x(), pos.y() - anchorPos.y());
        geometry.stations.append({it.key(), pos2D});
        name2D.insert(it.key(), pos2D);
    }

    // Shot lines from network adjacency. De-dupe by sorted endpoint pair.
    QSet<QString> seenEdges;
    for (const auto &from : component.network.stations()) {
        const auto neighbors = component.network.neighbors(from);
        for (const auto &to : neighbors) {
            QString a = cwStation::canonicalKey(from);
            QString b = cwStation::canonicalKey(to);
            if (a > b) {
                std::swap(a, b);
            }
            const QString edgeKey = a + '\0' + b;
            if (seenEdges.contains(edgeKey)) {
                continue;
            }
            seenEdges.insert(edgeKey);

            const auto itFrom = name2D.constFind(from);
            const auto itTo   = name2D.constFind(to);
            if (itFrom == name2D.constEnd() || itTo == name2D.constEnd()) {
                continue;
            }
            geometry.shotLines.append(QLineF(itFrom.value(), itTo.value()));
        }
    }
    return geometry;
}

} // namespace

void cwSketchCanvas::refreshLinePlotFromManager()
{
    if (m_sketch == nullptr || m_sketchManager.isNull() || m_acquiredTrip.isNull()) {
        clearLinePlotGeometry();
        return;
    }

    const auto components = m_sketchManager->latestLinePlot(m_acquiredTrip.data());
    if (components.isEmpty()) {
        clearLinePlotGeometry();
        return;
    }

    QString anchor = m_sketch->anchorStation();

    if (components.size() == 1 && anchor.isEmpty()) {
        m_sketch->setAnchorStation(components.first().canonicalAnchor);
        anchor = m_sketch->anchorStation();
    }

    int componentIndex = indexOfComponentContainingAnchor(components, anchor);

    // Stale anchor: caver deleted/renamed the station we remembered. Fall
    // back to the largest component rather than prompt — less disruptive
    // mid-edit than a modal.
    if (componentIndex < 0 && !anchor.isEmpty()) {
        qCWarning(lcSketchCanvas) << "anchorStation" << anchor
                                  << "not present in any component of trip"
                                  << m_acquiredTrip->name()
                                  << "— falling back to largest component";
        m_sketch->setAnchorStation(components.first().canonicalAnchor);
        anchor = m_sketch->anchorStation();
        componentIndex = 0;
    }

    if (componentIndex < 0) {
        if (components.size() > 1) {
            emit requestAnchorSelection(componentSummaries(components));
        }
        clearLinePlotGeometry();
        return;
    }

    cwSurvey2DGeometry geometry;
    switch (m_sketch->viewType()) {
    case cwSketch::Plan:
        geometry = projectPlan(components.at(componentIndex), anchor);
        break;
    case cwSketch::RunningProfile:
    case cwSketch::ProjectedProfile:
        // Stubbed — per plan §5; implement when those view types ship.
        break;
    }

    m_linePlotGeometry->setGeometryResult(
        AsyncFuture::completed(Monad::Result<cwSurvey2DGeometry>(std::move(geometry))));
}
