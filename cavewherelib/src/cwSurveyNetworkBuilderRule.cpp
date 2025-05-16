#include "cwSurveyNetworkBuilderRule.h"
#include "cwConcurrent.h"

//Qt includes
#include <QtMath>    // for qDegreesToRadians()

//Std includes
#include <cmath>     // for std::fmod()

using namespace Monad;

cwSurveyNetworkBuilderRule::cwSurveyNetworkBuilderRule(QObject *parent)
    : cwAbstractRule(parent)
    , m_surveyNetworkArtifact(new cwSurveyNetworkArtifact(this))
{
    setName("Survey Network Builder");
}

void cwSurveyNetworkBuilderRule::setSurveyData(cwSurveyDataArtifact *surveyData)
{
    if (m_surveyData != surveyData) {
        if(!m_surveyData.isNull()) {
            disconnect(m_surveyData, nullptr, this, nullptr);
        }

        m_surveyData = surveyData;

        if(!m_surveyData.isNull()) {
            connect(m_surveyData, &cwSurveyDataArtifact::surveyDataChanged, this, &cwSurveyNetworkBuilderRule::updatePipeline);
            updatePipeline();
        }

        emit surveyDataChanged();
    }
}

void cwSurveyNetworkBuilderRule::updatePipeline()
{
    if(m_surveyData->region()) {

        const auto region = cwSurveyDataArtifact::Region(m_surveyData->region()); //Copy all the region data for export

        auto future = cwConcurrent::run([region = std::move(region)]()->Result<cwSurveyNetwork> {

            auto nameWithPrefix = [](const cwSurveyDataArtifact::Cave& cave, const cwStation& station)->QString {
                return QString(cave.name + QStringLiteral(".") + station.name()).toLower();
            };

            auto toStationPosition = [](const cwSurveyDataArtifact::SurveyChunk& chunk, int shotIndex) -> QVector3D {
                const cwShot& shot = chunk.shots.at(shotIndex);
                const int count = shot.measurementCount();
                if (count == 0) {
                    return QVector3D(); // no measurements → zero offset
                }

                struct Mean {
                    double sum = 0.0;
                    double count = 0.0;

                    double mean() const {
                        if(count > 0.0) {
                            return sum / count;
                        }
                        return 0.0;
                    }

                    void addValue(double value) {
                        sum += value;
                        ++count;
                    }
                };

                Mean distanceMean;
                Mean azimuthMean;
                Mean clinoMean;

                auto addMeasument = [](Mean* mean,
                                       const auto& reading,
                                       cwShotMeasurement::Direction direction,
                                       auto validState,
                                       auto flip) {
                    if(reading.state() == validState) {
                        double value = reading.toDouble();
                        if(direction == cwShotMeasurement::Direction::Back) {
                            value = flip(value);
                        }
                        mean->addValue(value);
                    }
                };

                for (int i = 0; i < count; ++i) {
                    const auto& measurement = shot.measurementAt(i);

                    addMeasument(&distanceMean,
                                 measurement.distance,
                                 measurement.direction,
                                 cwDistanceReading::State::Valid,
                                 //Passthrough for backsight
                                 [](double value) { return value; }
                                 );

                    addMeasument(
                        &azimuthMean,
                        measurement.compass,
                        measurement.direction,
                        cwCompassReading::State::Valid,
                        //Flip for backsight
                        [](double value) { return std::fmod(value + 180.0, 360.0); }
                        );

                    addMeasument(
                        &clinoMean,
                        measurement.clino,
                        measurement.direction,
                        cwClinoReading::State::Valid,
                        //Flip for backsight
                        [](double value) { return -value; }
                        );
                }

                // compute the averages
                double avgDistance = distanceMean.mean();
                double avgAzimuth = azimuthMean.mean();
                double avgClino = clinoMean.mean();

                // convert to radians
                const double azimuthRad = qDegreesToRadians(avgAzimuth);
                const double clinoRad = qDegreesToRadians(avgClino);

                // spherical→Cartesian (x=east, y=north, z=up)
                double x = avgDistance * std::cos(clinoRad) * std::sin(azimuthRad);
                double y = avgDistance * std::cos(clinoRad) * std::cos(azimuthRad);
                double z = avgDistance * std::sin(clinoRad);

                return QVector3D(x, y, z);
            };

            struct ShotVectors {
            public:
                [[nodiscard]] bool hasVector(const QString& from,
                                             const QString& to) const
                {
                    return m_offsets.contains(makeKey(from, to));
                }

                [[nodiscard]] QVector3D vector(const QString& from,
                                               const QString& to) const
                {
                    return m_offsets.value(makeKey(from, to));
                }

                void add(const QString& from,
                         const QString& to,
                         const QVector3D& off)
                {
                    m_offsets.insert(makeKey(from, to), off);
                    m_offsets.insert(makeKey(to, from), -off);
                }

            private:
                QHash<QString, QVector3D> m_offsets;

                [[nodiscard]] QString makeKey(const QString& from,
                                              const QString& to) const
                {
                    // combine stations into one unique key
                    return from + QStringLiteral("-") + to;
                }
            };

            auto appendCaveToNetwork = [nameWithPrefix, toStationPosition](cwSurveyNetwork& network,
                                                                           ShotVectors& shotVectors,
                                                                           const cwSurveyDataArtifact::Cave& cave) {

                bool firstStationSet = false;
                QString firstValidStation;

                for(const auto& trip : cave.trips) {
                    for(const auto& chunk : trip.chunks) {
                        for(int i = 0; i < chunk.stations.size() - 1; i++) {
                            const cwStation from = chunk.stations.at(i);
                            const cwStation to = chunk.stations.at(i + 1);

                            if (!from.isValid() || !to.isValid()) {
                                continue;
                            }

                            if (!firstStationSet) {
                                firstValidStation = nameWithPrefix(cave, from);
                                network.setPosition(firstValidStation, {0.0, 0.0, 0.0});
                                firstStationSet   = true;
                            }

                            const auto fromName = nameWithPrefix(cave, from);
                            const auto toName = nameWithPrefix(cave, to);

                            network.addShot(fromName, toName);

                            QVector3D vector = toStationPosition(chunk, i);
                            shotVectors.add(fromName, toName, vector);
                            }
                        }
                    }


                // --- Pass 2: Breath First Search from root to assign positions ---
                std::queue<QString> frontier;
                frontier.push(firstValidStation);

                while (!frontier.empty()) {
                    QString current = frontier.front();
                    frontier.pop();

                    const auto neigbors = network.neighbors(current);
                    for (auto const& neighbor : neigbors) {
                        if (network.hasPosition(neighbor)) {
                            continue;
                        }
                        if (!shotVectors.hasVector(current, neighbor)) {
                            continue;
                        }

                        QVector3D currentPos = network.position(current);
                        QVector3D vector = shotVectors.vector(current, neighbor);
                        network.setPosition(neighbor, currentPos + vector);
                        frontier.push(neighbor);
                    }
                }

                return network;
            };

            ShotVectors shotVectors;
            cwSurveyNetwork network;

            for(const auto& cave : std::as_const(region.caves)) {
                appendCaveToNetwork(network, shotVectors, cave);
            }

            return network;
        });

        m_surveyNetworkArtifact->setSurveyNetwork(future);
    }
}
