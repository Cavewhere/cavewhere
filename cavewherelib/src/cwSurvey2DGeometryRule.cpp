// cwSurvey2DGeometryRule.cpp

//Our includes
#include "cwSurvey2DGeometryRule.h"
#include "cwSurvey2DGeometry.h"
#include "cwConcurrent.h"

//Async future
#include "asyncfuture.h"
using namespace AsyncFuture;
using namespace Monad;

cwSurvey2DGeometryRule::cwSurvey2DGeometryRule(QObject* parent)
    : cwAbstractRule(parent),
    m_survey2DGeometry(new cwSurvey2DGeometryArtifact(this))
{
    setName("Survey2DGeometryRule");

    connect(this, &cwSurvey2DGeometryRule::surveyNetworkChanged,
            this, &cwSurvey2DGeometryRule::updateGeometry);
    connect(this, &cwSurvey2DGeometryRule::viewMatrixChanged,
            this, &cwSurvey2DGeometryRule::updateGeometry);
}

cwSurveyNetworkArtifact* cwSurvey2DGeometryRule::surveyNetwork() const
{
    return m_surveyNetwork;
}

void cwSurvey2DGeometryRule::setSurveyNetwork(
    cwSurveyNetworkArtifact* surveyNetwork)
{
    if (m_surveyNetwork != surveyNetwork) {
        if(m_surveyNetwork) {
            disconnect(m_surveyNetwork, nullptr, this, nullptr);
        }

        m_surveyNetwork = surveyNetwork;

        if(m_surveyNetwork) {
            connect(m_surveyNetwork, &cwSurveyNetworkArtifact::surveyNetworkChanged,
                    this, &cwSurvey2DGeometryRule::updateGeometry);
        }

        emit surveyNetworkChanged();
    }
}

cwMatrix4x4Artifact* cwSurvey2DGeometryRule::viewMatrix() const
{
    return m_viewMatrix;
}

void cwSurvey2DGeometryRule::setViewMatrix(cwMatrix4x4Artifact *viewMatrix)
{
    if (m_viewMatrix != viewMatrix) {
        if(m_viewMatrix) {
            disconnect(m_viewMatrix, nullptr, this, nullptr);
        }

        m_viewMatrix = viewMatrix;

        if(m_viewMatrix) {
            connect(m_viewMatrix, &cwMatrix4x4Artifact::matrix4x4Changed,
                    this, &cwSurvey2DGeometryRule::updateGeometry);
        }

        emit viewMatrixChanged();
    }
}

cwSurvey2DGeometryArtifact* cwSurvey2DGeometryRule::survey2DGeometry() const
{
    return m_survey2DGeometry;
}

QList<QByteArray> cwSurvey2DGeometryRule::sourcesNames() const
{
    return { "surveyNetwork", "viewMatrix" };
}

QList<QByteArray> cwSurvey2DGeometryRule::outputsNames() const
{
    return { "survey2DGeometry" };
}

void cwSurvey2DGeometryRule::updateGeometry()
{
    if (!m_surveyNetwork || !m_viewMatrix) {
        return;
    }

    auto surveyNetworkFuture = m_surveyNetwork->surveyNetwork();
    auto matrix = m_viewMatrix->matrix4x4();

    int value = 5;
    // Result<double> final = mbind(Result(value), [](Result<int> value) { return Result<double>(5.0); });

    auto createGeometry = [matrix](QFuture<Result<cwSurveyNetwork>> surveyNetworkFuture) {
        return cwConcurrent::run([=]() {
            cwSurvey2DGeometry geometry;
            const auto& surveyNetwork = surveyNetworkFuture.result().value();

            const auto stationNames = surveyNetwork.stations();
            geometry.stations.reserve(stationNames.size());

            QSet<QString> shotLinesAdded;
            auto hasShotLine = [&shotLinesAdded](const QString& from, const QString& to) {
                if(shotLinesAdded.contains(from + to)) {
                    return true;
                } else if(shotLinesAdded.contains(to + from)) {
                    return true;
                } else {
                    return false;
                }
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
    // setValid(true);
    emit survey2DGeometryChanged();
}
