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

//Monad / async
#include "Monad/Monad.h"
#include "asyncfuture.h"

//Qt includes
#include <QFont>

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

    auto convertToPainterPaths = [](const Result<cwSurvey2DGeometry> result) -> QFuture<Result<QVector<Path>>> {
        const cwSurvey2DGeometry geometry = result.value();
        return cwConcurrent::run([geometry]() {
            QVector<Path> paths;
            paths.reserve(3);

            // Shot lines.
            {
                QPainterPath lines;
                for (const auto &line : geometry.shotLines) {
                    lines.moveTo(line.p1());
                    lines.lineTo(line.p2());
                }
                Path p;
                p.painterPath = std::move(lines);
                p.strokeWidth = 1.0;
                paths.append(std::move(p));
            }

            // Station symbols (filled dots).
            {
                QPainterPath symbols;
                for (const auto &station : geometry.stations) {
                    symbols.addEllipse(station.position, 1.0, 1.0);
                }
                Path p;
                p.painterPath = std::move(symbols);
                p.strokeWidth = 1.0;
                paths.append(std::move(p));
            }

            // Station labels.
            {
                QPainterPath labels;
                QFont font;
                font.setPointSizeF(10.0);
                for (const auto &station : geometry.stations) {
                    labels.addText(station.position + QPointF(2.0, 2.0),
                                   font,
                                   station.name);
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
