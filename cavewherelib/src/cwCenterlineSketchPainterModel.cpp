/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCenterlineSketchPainterModel.h"
#include "cwSurvey2DGeometryArtifact.h"
#include "cwSurvey2DGeometry.h"
#include "cwConcurrent.h"
#include "cwScale.h"

//Monad / async
#include "Monad/Monad.h"
#include "asyncfuture.h"

//Qt includes
#include <QFont>
#include <QFontMetricsF>
#include <QTransform>

using namespace AsyncFuture;
using namespace Monad;

cwCenterlineSketchPainterModel::cwCenterlineSketchPainterModel(QObject *parent)
    : cwAbstractSketchPainterPathModel(parent)
{
}

void cwCenterlineSketchPainterModel::setSurvey2DGeometry(cwSurvey2DGeometryArtifact *geometry)
{
    if (m_geometryArtifact == geometry) {
        return;
    }

    if (m_geometryArtifact) {
        disconnect(m_geometryArtifact,
                   &cwSurvey2DGeometryArtifact::geometryResultChanged,
                   this,
                   &cwCenterlineSketchPainterModel::updateModel);
    }

    m_geometryArtifact = geometry;

    if (m_geometryArtifact) {
        connect(m_geometryArtifact,
                &cwSurvey2DGeometryArtifact::geometryResultChanged,
                this,
                &cwCenterlineSketchPainterModel::updateModel);
    }

    emit survey2DGeometryChanged();
    updateModel();
}

void cwCenterlineSketchPainterModel::setMapScale(cwScale *scale)
{
    if (m_mapScale == scale) {
        return;
    }
    if (m_mapScale) {
        disconnect(m_mapScale, &cwScale::scaleChanged,
                   this, &cwCenterlineSketchPainterModel::updateModel);
    }
    m_mapScale = scale;
    if (m_mapScale) {
        connect(m_mapScale, &cwScale::scaleChanged,
                this, &cwCenterlineSketchPainterModel::updateModel);
    }
    emit mapScaleChanged();
    updateModel();
}

int cwCenterlineSketchPainterModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_paths.size();
}

cwAbstractSketchPainterPathModel::Path cwCenterlineSketchPainterModel::path(const QModelIndex &index) const
{
    return m_paths.at(index.row());
}

void cwCenterlineSketchPainterModel::updateModel()
{
    if (!m_geometryArtifact) {
        beginResetModel();
        m_paths.clear();
        endResetModel();
        return;
    }

    auto future = m_geometryArtifact->geometryResult();

    // Map-scale ratio (e.g. 1/250 = 0.004). Falls back to 1:250 if no
    // mapScale is attached — matches the default used in SketchItem.qml.
    const double scaleRatio = (m_mapScale && m_mapScale->scale() > 0.0)
        ? m_mapScale->scale()
        : (1.0 / 250.0);

    // Paper-unit geometry is sized in paper millimetres and converted to
    // world meters so it stays the same size on paper across map scales:
    //   world_m = paper_mm / (1000 * scaleRatio)
    // e.g. at 1:250, 1 mm paper ≈ 0.25 m world.
    constexpr double stationSymbolDiameterPaperMm = 1.0; // 0.5 mm radius
    constexpr double stationLabelHeightPaperMm    = 3.0;
    constexpr double shotLineWidthWorldMeters     = 0.02;

    const double stationRadiusWorld =
        0.5 * stationSymbolDiameterPaperMm / (1000.0 * scaleRatio);
    const double labelTargetHeightWorld =
        stationLabelHeightPaperMm / (1000.0 * scaleRatio);

    auto convertToPainterPaths = [stationRadiusWorld,
                                  labelTargetHeightWorld]
        (const Result<cwSurvey2DGeometry> result) -> QFuture<Result<QVector<Path>>> {
        const cwSurvey2DGeometry geometry = result.value();
        return cwConcurrent::run([geometry,
                                  stationRadiusWorld,
                                  labelTargetHeightWorld]() {
            QVector<Path> paths;
            paths.reserve(3);

            // Shot lines — stroke width specified in world meters.
            {
                QPainterPath lines;
                for (const auto &line : geometry.shotLines) {
                    lines.moveTo(line.p1());
                    lines.lineTo(line.p2());
                }
                Path p;
                p.painterPath = std::move(lines);
                p.strokeWidth = shotLineWidthWorldMeters;
                paths.append(std::move(p));
            }

            // Station symbols (filled dots) — radius in world meters derived
            // from the paper-mm constant above.
            {
                QPainterPath symbols;
                for (const auto &station : geometry.stations) {
                    symbols.addEllipse(station.position,
                                       stationRadiusWorld,
                                       stationRadiusWorld);
                }
                Path p;
                p.painterPath = std::move(symbols);
                // Filled pass.
                p.strokeWidth = 0.0;
                paths.append(std::move(p));
            }

            // Station labels — addText bakes glyphs in Qt-internal font
            // units; we measure their natural height and apply a
            // per-label QTransform so the rendered height in world meters
            // matches labelTargetHeightWorld (paper-mm derived). Y is
            // flipped so the baseline sits above the station point (world
            // Y is up).
            {
                QPainterPath labels;
                QFont font;
                font.setPointSizeF(10.0);
                const QFontMetricsF metrics(font);
                const double naturalHeight = std::max(1.0, metrics.height());
                const double glyphScale    = labelTargetHeightWorld / naturalHeight;

                // Offset label slightly right + above the station position
                // (world Y is up, so positive offset moves up visually).
                const double labelOffsetWorld = stationRadiusWorld * 2.0;

                for (const auto &station : geometry.stations) {
                    QPainterPath glyph;
                    glyph.addText(QPointF(0, 0), font, station.name);
                    QTransform t;
                    t.translate(station.position.x() + labelOffsetWorld,
                                station.position.y() + labelOffsetWorld);
                    // Flip Y so the glyph sits above the anchor in a
                    // world-Y-up coordinate system.
                    t.scale(glyphScale, -glyphScale);
                    labels.addPath(t.map(glyph));
                }
                Path p;
                p.painterPath = std::move(labels);
                // Stroke width 0 flags this row as a fill pass; station
                // labels are rendered by filling the text path, not stroking.
                p.strokeWidth = 0.0;
                paths.append(std::move(p));
            }

            return Result(paths);
        });
    };

    auto painterPathsObserver = observe(future).context(this, [future, convertToPainterPaths]() {
        return mbind(future, convertToPainterPaths);
    });

    painterPathsObserver.context(this, [painterPathsObserver, this]() {
        const auto pathFuture = painterPathsObserver.future();
        if (!pathFuture.isFinished() || pathFuture.isCanceled()) {
            return;
        }
        if (pathFuture.result().hasError()) {
            return;
        }
        beginResetModel();
        m_paths = pathFuture.result().value();
        endResetModel();
    });
}
