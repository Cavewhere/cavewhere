// cwSurvey2DGeometryBuilder.cpp

//Our includes
#include "cwSurvey2DGeometryBuilder.h"
#include "cwSurvey2DGeometry.h"
#include "cwConcurrent.h"

//Async future
#include "asyncfuture.h"
using namespace AsyncFuture;
using namespace Monad;

cwSurvey2DGeometryBuilder::cwSurvey2DGeometryBuilder(QObject* parent)
    : QObject(parent),
    m_survey2DGeometry(new cwSurvey2DGeometrySource(this))
{
}

cwSurveyNetworkSource* cwSurvey2DGeometryBuilder::surveyNetwork() const
{
    return m_surveyNetwork;
}

void cwSurvey2DGeometryBuilder::setSurveyNetwork(cwSurveyNetworkSource* surveyNetwork)
{
    if (m_surveyNetwork == surveyNetwork) {
        return;
    }

    if (m_surveyNetwork) {
        disconnect(m_surveyNetwork, &cwSurveyNetworkSource::surveyNetworkChanged,
                   this, &cwSurvey2DGeometryBuilder::updateGeometry);
    }

    m_surveyNetwork = surveyNetwork;

    if (m_surveyNetwork) {
        connect(m_surveyNetwork, &cwSurveyNetworkSource::surveyNetworkChanged,
                this, &cwSurvey2DGeometryBuilder::updateGeometry);
    }

    emit surveyNetworkChanged();
    updateGeometry();
}

QMatrix4x4 cwSurvey2DGeometryBuilder::viewMatrix() const
{
    return m_viewMatrix;
}

void cwSurvey2DGeometryBuilder::setViewMatrix(const QMatrix4x4& matrix)
{
    if (m_viewMatrix == matrix) {
        return;
    }

    m_viewMatrix = matrix;
    emit viewMatrixChanged();
    updateGeometry();
}

cwSurvey2DGeometrySource* cwSurvey2DGeometryBuilder::survey2DGeometry() const
{
    return m_survey2DGeometry;
}

void cwSurvey2DGeometryBuilder::updateGeometry()
{
    if (!m_surveyNetwork) {
        return;
    }

    auto surveyNetworkFuture = m_surveyNetwork->surveyNetwork();
    const QMatrix4x4 matrix = m_viewMatrix;

    auto createGeometry = [matrix](const Result<cwSurveyNetwork> surveyNetworkResult) {
        return cwConcurrent::run([=]() {
            cwSurvey2DGeometry geometry;
            const auto surveyNetwork = surveyNetworkResult.value();

            const auto stationNames = surveyNetwork.stations();
            geometry.stations.reserve(stationNames.size());

            QSet<QString> shotLinesAdded;
            auto hasShotLine = [&shotLinesAdded](const QString& from, const QString& to) {
                return shotLinesAdded.contains(from + to)
                       || shotLinesAdded.contains(to + from);
            };

            for(const auto& name : stationNames) {
                auto position3d = surveyNetwork.position(name);
                auto position = matrix.map(position3d).toPointF();
                geometry.stations.append({name, position});

                const auto neighbors = surveyNetwork.neighbors(name);
                for (const QString& toName : neighbors) {
                    if(!hasShotLine(name, toName)) {
                        auto toPosition3d = surveyNetwork.position(toName);
                        auto toPosition = matrix.map(toPosition3d).toPointF();

                        geometry.shotLines.append(QLineF(position, toPosition));
                        shotLinesAdded.insert(name + toName);
                    }
                }
            }
            return Result(geometry);
        });
    };

    auto future = observe(surveyNetworkFuture).context(this, [createGeometry, surveyNetworkFuture]() {
                                                  return mbind(surveyNetworkFuture, createGeometry);
                                              }).future();

    m_survey2DGeometry->setGeometryResult(future);
}
