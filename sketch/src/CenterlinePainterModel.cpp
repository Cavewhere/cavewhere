// CenterlinePainterModel.cpp
#include "CenterlinePainterModel.h"

// CaveWhere
#include "cwConcurrent.h"

// Monad
#include "Monad/Monad.h"

// Async
#include "asyncfuture.h"
using namespace AsyncFuture;
using namespace cwSketch;
using namespace Monad;


CenterlinePainterModel::CenterlinePainterModel(QObject *parent)
    : AbstractPainterPathModel(parent)
{}

cwSurvey2DGeometrySource* CenterlinePainterModel::survey2DGeometry() const
{
    return m_geometrySource;
}

void CenterlinePainterModel::setSurvey2DGeometry(cwSurvey2DGeometrySource* geometry)
{
    if (m_geometrySource == geometry) {
        return;
    }

    if (m_geometrySource) {
        disconnect(m_geometrySource,
                   &cwSurvey2DGeometrySource::geometryResultChanged,
                   this,
                   &CenterlinePainterModel::updateModel);
    }

    m_geometrySource = geometry;

    if (m_geometrySource) {
        connect(m_geometrySource,
                &cwSurvey2DGeometrySource::geometryResultChanged,
                this,
                &CenterlinePainterModel::updateModel);
    }

    emit survey2DGeometryChanged();
    updateModel();
}

int CenterlinePainterModel::rowCount(const QModelIndex &parent) const
{
    return m_paths.size();
}

AbstractPainterPathModel::Path CenterlinePainterModel::path(const QModelIndex &index) const
{
    return m_paths.at(index.row());
}

void CenterlinePainterModel::updateModel()
{
    if (m_geometrySource) {
        // pull the future and wait for it
        auto future = m_geometrySource->geometryResult();

        auto convertToPainterPaths = [](const auto result)->QFuture<Result<QVector<Path>>> {
            const cwSurvey2DGeometry geometry = result.value();
            return cwConcurrent::run([geometry]() {
                QVector<Path> paths;
                paths.reserve(3);

                // shot‐lines path
                {
                    Path linePath;
                    QPainterPath lines;
                    for (auto const &line : geometry.shotLines) {
                        lines.moveTo(line.p1());
                        lines.lineTo(line.p2());
                        qDebug() << "Line:" << line.p1() << line.p2();
                    }
                    linePath.painterPath = std::move(lines);
                    linePath.strokeWidth = 1.0;
                    paths.append(std::move(linePath));
                }

                // station symbols (circles)
                {
                    Path symbolPath;
                    QPainterPath symbols;
                    for (auto const &station : geometry.stations) {
                        symbols.addEllipse(station.position, 1.0, 1.0);
                    }
                    symbolPath.painterPath = std::move(symbols);
                    symbolPath.strokeWidth = 1.0;
                    paths.append(std::move(symbolPath));
                }

                // station labels
                {
                    Path labelPath;
                    QPainterPath labels;
                    QFont font;
                    font.setPointSizeF(10.0);
                    for (auto const &station : geometry.stations) {
                        // offset label by (0.6, 0.2)
                        labels.addText(station.position + QPointF(2.0, 2.0),
                                       font,
                                       station.name);
                    }
                    labelPath.painterPath = std::move(labels);
                    labelPath.strokeWidth = 0.0;
                    paths.append(std::move(labelPath));
                }

                return Result(paths);
            });
        };

        auto painterPathsObserver = observe(future).context(this, [future, convertToPainterPaths]() {
            return mbind(future, convertToPainterPaths);
        });

        painterPathsObserver.context(this, [painterPathsObserver, this]() {
            const auto pathFuture = painterPathsObserver.future();
            Q_ASSERT(pathFuture.isFinished() && !pathFuture.isCanceled());

            if(!pathFuture.result().hasError()) {
                beginResetModel();
                m_paths = pathFuture.result().value();
                endResetModel();
            }
        });
    }
}

