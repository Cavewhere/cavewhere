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
    : QObject(parent)
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

void cwCenterlineSketchPainterModel::setStationColor(const QColor &color)
{
    if (m_stationColor == color) {
        return;
    }
    m_stationColor = color;
    emit stationColorChanged();
    scheduleColorUpdate();
}

void cwCenterlineSketchPainterModel::setShotLineColor(const QColor &color)
{
    if (m_shotLineColor == color) {
        return;
    }
    m_shotLineColor = color;
    emit shotLineColorChanged();
    scheduleColorUpdate();
}

// Coalesce station+shotLine writes that arrive in the same event-loop turn
// (a theme toggle fires both bindings) into a single updateModel() so the
// async snapshot pipeline only runs once per theme change.
void cwCenterlineSketchPainterModel::scheduleColorUpdate()
{
    if (m_colorUpdatePending) {
        return;
    }
    m_colorUpdatePending = true;
    QMetaObject::invokeMethod(this, [this]() {
        m_colorUpdatePending = false;
        updateModel();
    }, Qt::QueuedConnection);
}

void cwCenterlineSketchPainterModel::updateModel()
{
    if (!m_geometryArtifact) {
        m_paths.clear();
        emit pathsChanged();
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
    constexpr double shotLineWidthPaperMm         = 0.25;

    const double paperMmToWorldM = 1.0 / (1000.0 * scaleRatio);

    const double stationRadiusWorld =
        0.5 * stationSymbolDiameterPaperMm * paperMmToWorldM;
    const double labelTargetHeightWorld =
        stationLabelHeightPaperMm * paperMmToWorldM;
    const double shotLineWidthWorld =
        shotLineWidthPaperMm * paperMmToWorldM;

    struct Snapshot {
        QList<Path> paths;
        QVector<cwGridTextModel::TextRow> textRows;
    };

    // Capture colors by value into the worker; QColor is a trivially-copyable
    // value type and the worker must not touch this object's members.
    const QColor stationColor = m_stationColor;
    const QColor shotLineColor = m_shotLineColor;

    auto convertToPainterPaths = [stationRadiusWorld,
                                  labelTargetHeightWorld,
                                  shotLineWidthWorld,
                                  stationColor,
                                  shotLineColor]
        (const Result<cwSurvey2DGeometry> result) -> QFuture<Result<Snapshot>> {
        const cwSurvey2DGeometry geometry = result.value();
        return cwConcurrent::run([geometry,
                                  stationRadiusWorld,
                                  labelTargetHeightWorld,
                                  shotLineWidthWorld,
                                  stationColor,
                                  shotLineColor]() {
            // strokeWidth == 0 flags a fill pass (see cwSketchPathSource).
            auto makePath = [](QPainterPath path, const QColor &color, double strokeWidth) {
                Path p;
                p.painterPath = std::move(path);
                p.strokeColor = color;
                p.strokeWidth = strokeWidth;
                return p;
            };

            Snapshot snapshot;
            snapshot.paths.reserve(2);

            QPainterPath lines;
            for (const auto &line : geometry.shotLines) {
                lines.moveTo(line.p1());
                lines.lineTo(line.p2());
            }
            snapshot.paths.append(makePath(std::move(lines), shotLineColor, shotLineWidthWorld));

            QPainterPath symbols;
            for (const auto &station : geometry.stations) {
                symbols.addEllipse(station.position,
                                   stationRadiusWorld,
                                   stationRadiusWorld);
            }
            snapshot.paths.append(makePath(std::move(symbols), stationColor, 0.0));

            // The shared painter text helper anchors glyphs at
            // bounds.topLeft; a non-zero height would shift the Y-flipped
            // screen anchor by that height, so pass a degenerate rect.
            Q_UNUSED(labelTargetHeightWorld);
            QFont font;
            font.setPointSizeF(12.0);
            const double labelOffsetWorld = stationRadiusWorld * 2.0;

            snapshot.textRows.reserve(geometry.stations.size());
            for (const auto &station : geometry.stations) {
                const QPointF worldAnchor(
                    station.position.x() + labelOffsetWorld,
                    station.position.y() + labelOffsetWorld);

                cwGridTextModel::TextRow row;
                row.text        = station.name;
                row.bounds      = QRectF(worldAnchor, QSizeF(0.0, 0.0));
                row.position    = worldAnchor;
                row.font        = font;
                row.fillColor   = stationColor;
                row.strokeColor = Qt::transparent;
                snapshot.textRows.append(row);
            }

            return Result(snapshot);
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
        auto snapshot = pathFuture.result().value();
        m_paths = std::move(snapshot.paths);
        emit pathsChanged();
        if (m_textRows != snapshot.textRows) {
            m_textRows = std::move(snapshot.textRows);
            emit textRowsChanged();
        }
    });
}
