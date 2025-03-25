//Our includes
#include "cwSurveyEditorModel.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwTripCalibration.h"

cwSurveyEditorModel::cwSurveyEditorModel()
{

}

/**
* @brief cwSurveyEditorModel::setTrip
* @param trip -
*/
// void cwSurveyEditorModel::setTrip(cwTrip* trip) {
//     if(Trip != trip) {
//         beginResetModel();
//         Trip = trip;

//         if(Trip) {
//             auto connectChunk = [this](cwSurveyChunk* chunk) {
//                 connect(chunk, &cwSurveyChunk::dataChanged, this, [this, chunk](cwSurveyChunk::DataRole role, int stationIndex) {
//                     for (auto it = IndexToChunk.begin(); it != IndexToChunk.end(); ++it) {
//                         if (it.value() == chunk) {
//                             int modelRow = it.key().low() + stationIndex;
//                             QModelIndex idx = index(modelRow, 0);
//                             emit dataChanged(idx, idx, QVector<int>() << role);
//                             break;
//                         }
//                     }
//                 });
//             };

//             // Connect to cwTrip signals to handle chunks being added or removed.
//             connect(Trip, &cwTrip::chunksInserted, this, [this, connectChunk](int begin, int end) {
//                 beginInsertRows(QModelIndex(), begin, end);
//                 updateIndexToChunk();
//                 // Connect the dataChanged signal for each newly inserted chunk.
//                 QList<cwSurveyChunk*> chunks = Trip->chunks();
//                 for (int i = begin; i <= end && i < chunks.size(); ++i) {
//                     cwSurveyChunk* chunk = chunks.at(i);
//                     connectChunk(chunk);
//                 }
//                 endInsertRows();
//             });

//             connect(Trip, &cwTrip::chunksAboutToBeRemoved, this, [this](int begin, int end) {
//                 beginRemoveRows(QModelIndex(), begin, end);
//             });

//             connect(Trip, &cwTrip::chunksRemoved, this, [this](int begin, int end) {
//                 updateIndexToChunk();
//                 endRemoveRows();
//             });

//             // Connect to dataChanged signal for existing chunks.
//             const auto chunks = Trip->chunks();
//             for (cwSurveyChunk* chunk : chunks) {
//                 connectChunk(chunk);
//             }
//         }

//         updateIndexToChunk();
//         endResetModel();


//         emit tripChanged();
//     }
// }

void cwSurveyEditorModel::setTrip(cwTrip* trip) {
    if(m_trip != trip) {
        beginResetModel();
        m_trip = trip;

        if(m_trip) {
            // Lambda to connect signals for a cwSurveyChunk.
            auto connectChunk = [this](cwSurveyChunk* chunk) {
                // Data changes in the chunk.


                connect(chunk, &cwSurveyChunk::dataChanged, this,
                        [this, chunk](cwSurveyChunk::DataRole role, int stationIndex) {
                            auto modelIndex = chunkIndexToModelIndex(chunk, stationIndex);
                            Q_ASSERT(modelIndex.isValid());
                            emit dataChanged(modelIndex, modelIndex, {chunkRoleToModelRole(role)});
                        });

                // auto findRowIndex = [this](const cwSurveyChunk* chunk) {
                //     // Compute base model row for this chunk.
                //     int baseRow = m_skipRowOffset;
                //     const QList<cwSurveyChunk*> allChunks = m_trip->chunks();
                //     for (cwSurveyChunk* c : allChunks) {
                //         if (c == chunk) {
                //             break;
                //         }
                //         baseRow += c->stationCount() + m_skipRowOffset;
                //     }
                //     return baseRow;
                // };

                // Stations added.
                connect(chunk, &cwSurveyChunk::stationsAdded, this,
                        [this, chunk](int beginIndex, int endIndex) {
                            int insertRow = chunkIndexToRow(chunk, beginIndex);
                            beginInsertRows(QModelIndex(), insertRow, insertRow + (endIndex - beginIndex));
                            updateIndexToChunk();
                            endInsertRows();
                        });
                // Stations removed.
                connect(chunk, &cwSurveyChunk::stationsRemoved, this,
                        [this, chunk](int beginIndex, int endIndex) {
                            int removeRow = chunkIndexToRow(chunk, beginIndex);
                            beginRemoveRows(QModelIndex(), removeRow, removeRow + (endIndex - beginIndex));
                            updateIndexToChunk();
                            endRemoveRows();
                        });
                // // Shots added (row count remains unchanged; emit dataChanged).
                // connect(chunk, &cwSurveyChunk::shotsAdded, this,
                //         [this, chunk](int beginIndex, int endIndex) {
                //             int baseRow = 1;
                //             const QList<cwSurveyChunk*> allChunks = m_trip->chunks();
                //             for (cwSurveyChunk* c : allChunks) {
                //                 if (c == chunk) break;
                //                 baseRow += c->stationCount() + 1;
                //             }
                //             for (int i = beginIndex; i <= endIndex; ++i) {
                //                 int modelRow = baseRow + i;
                //                 QModelIndex idx = index(modelRow, 0);
                //                 emit dataChanged(idx, idx);
                //             }
                //         });
                // // Shots removed.
                // connect(chunk, &cwSurveyChunk::shotsRemoved, this,
                //         [this, chunk](int beginIndex, int endIndex) {
                //             int baseRow = 1;
                //             const QList<cwSurveyChunk*> allChunks = m_trip->chunks();
                //             for (cwSurveyChunk* c : allChunks) {
                //                 if (c == chunk) break;
                //                 baseRow += c->stationCount() + 1;
                //             }
                //             for (int i = beginIndex; i <= endIndex; ++i) {
                //                 int modelRow = baseRow + i;
                //                 QModelIndex idx = index(modelRow, 0);
                //                 emit dataChanged(idx, idx);
                //             }
                //         });
            };

            auto disconnectChunk = [this](cwSurveyChunk* chunk) {
                disconnect(chunk, nullptr, this, nullptr);
            };

            struct Range {
                int begin;
                int end;
            };

            auto modelRowRange = [this](int chunkBegin, int chunkEnd)->Range {
                int modelRow = 0;
                const QList<cwSurveyChunk*> allChunks = m_trip->chunks();
                for (int i = 0; i < chunkBegin && i < allChunks.size(); ++i) {
                    modelRow += allChunks.at(i)->stationCount() + m_skipRowOffset;
                }
                int totalNewRows = 0;
                for (int i = chunkBegin; i <= chunkEnd && i < allChunks.size(); ++i) {
                    totalNewRows += allChunks.at(i)->stationCount() + m_skipRowOffset;
                }
                return {modelRow, modelRow + totalNewRows - 1};
            };

            // Map cwTrip chunk-level changes to model row changes
            connect(m_trip, &cwTrip::chunksInserted, this,
                    [this, connectChunk, modelRowRange](int begin, int end) {
                        // Calculate the model row where insertion begins.
                        auto modelRange = modelRowRange(begin, end);

                        const QList<cwSurveyChunk*> allChunks = m_trip->chunks();
                        for (int i = begin; i <= end && i < allChunks.size(); ++i) {
                            connectChunk(allChunks.at(i));
                        }

                        beginInsertRows(QModelIndex(), modelRange.begin, modelRange.end);
                        endInsertRows();
                    });

            connect(m_trip, &cwTrip::chunksAboutToBeRemoved, this,
                    [this, disconnectChunk, modelRowRange](int begin, int end) {
                        auto modelRange = modelRowRange(begin, end);
                        beginRemoveRows(QModelIndex(), modelRange.begin, modelRange.end);

                        const QList<cwSurveyChunk*> allChunks = m_trip->chunks();
                        for (int i = begin; i <= end && i < allChunks.size(); ++i) {
                            disconnectChunk(allChunks.at(i));
                        }
                    });

            connect(m_trip, &cwTrip::chunksRemoved, this,
                    [this, disconnectChunk](int begin, int end) {
                        endRemoveRows();
                    });

            // Connect signals for all existing chunks.
            const auto chunks = m_trip->chunks();
            for (cwSurveyChunk* chunk : chunks) {
                connectChunk(chunk);
            }
        }

        updateIndexToChunk();
        endResetModel();

        emit tripChanged();
    }
}



QVariant cwSurveyEditorModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid()) {
        return QVariant();
    }

    auto chunkIndex = modelIndexToStationChunk(index);
    if(chunkIndex.chunk == nullptr) {
        switch(role) {
        case StationVisibleRole:
        case ShotVisibleRole:
            return false;
        case TitleVisibleRole:
            return true;
        default:
            return QVariant();
        }
    }

    Q_ASSERT(chunkIndex.index >= 0);

    cwSurveyChunk* chunk = chunkIndex.chunk;
    int stationIndex = chunkIndex.index;

    switch(role) {
    case StationNameRole:
        return chunk->station(stationIndex).name();
    case StationLeftRole:
        return QVariant::fromValue(chunk->station(stationIndex).left());
    case StationRightRole:
        return QVariant::fromValue(chunk->station(stationIndex).right());
    case StationUpRole:
        return QVariant::fromValue(chunk->station(stationIndex).up());
    case StationDownRole:
        return QVariant::fromValue(chunk->station(stationIndex).down());
    case ChunkRole:
        return QVariant::fromValue(chunk);
    // case ChunkIdRole:
    //     return QString("%1").arg(reinterpret_cast<qlonglong>(chunk));
    case StationVisibleRole:
        return true;
    case IndexInChunkRole:
        return stationIndex;
    case TitleVisibleRole:
        return false;
    default:
        break;
    }

    int shotIndex = stationIndex;

    if(shotIndex < chunk->shotCount()) {
        switch(role) {
        case ShotDistanceRole:
            return QVariant::fromValue(chunk->shot(shotIndex).distance());
        case ShotCompassRole:
            return QVariant::fromValue(chunk->shot(shotIndex).compass());
        case ShotBackCompassRole:
            return QVariant::fromValue(chunk->shot(shotIndex).backCompass());
        case ShotClinoRole:
            return QVariant::fromValue(chunk->shot(shotIndex).clino());
        case ShotBackClinoRole:
            return QVariant::fromValue(chunk->shot(shotIndex).backClino());
        case ShotCalibrationRole: {
            cwTripCalibration* calibration = chunk->calibrations().value(shotIndex);
            return QVariant::fromValue(calibration);
        }
        case ShotVisibleRole:
            return true;
        default:
            break;
        }
    } else {
        // qDebug() << "Shot count:" << shotIndex << chunk->shotCount() << role << DataBoxVisibleRole;

        if(role == ShotVisibleRole) {
            return false;
        }
    }

    return QVariant();
}

/**
 * @brief cwSurveyEditorModel::rowCount
 * @param parent
 * @return
 */
int cwSurveyEditorModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);

    if(m_trip.isNull()) {
        return 0;
    }

    auto chunks = m_trip->chunks();
    int count = 0;
    for(const auto chunk : std::as_const(chunks)) {
        count += chunk->stationCount() + m_skipRowOffset;
    }
    return count;

    // if(!m_indexToChunk.isEmpty()) {
    //     auto lastIter = std::prev(m_indexToChunk.end());
    //     return lastIter.key().high() + 1;
    // }
    // return 0;
}

/**
 * @brief cwSurveyEditorModel::roleNames
 * @return
 */
QHash<int, QByteArray> cwSurveyEditorModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(StationNameRole, "stationName");
    roles.insert(StationLeftRole, "stationLeft");
    roles.insert(StationRightRole, "stationRight");
    roles.insert(StationUpRole, "stationUp");
    roles.insert(StationDownRole, "stationDown");
    roles.insert(ShotDistanceRole, "shotDistance");
    roles.insert(ShotCompassRole, "shotCompass");
    roles.insert(ShotBackCompassRole, "shotBackCompass");
    roles.insert(ShotClinoRole, "shotClino");
    roles.insert(ShotBackClinoRole, "shotBackClino");
    roles.insert(ShotCalibrationRole, "shotCalibration");
    roles.insert(ChunkRole, "chunk");
    roles.insert(StationVisibleRole, "stationVisible");
    roles.insert(ShotVisibleRole, "shotVisible");
    roles.insert(TitleVisibleRole, "titleVisible");
    roles.insert(IndexInChunkRole, "indexInChunk");
    return roles;
}

/**
 * @brief cwSurveyEditorModel::addShotCalibration
 * @param index - The index for the whole trip
 */
void cwSurveyEditorModel::addShotCalibration(int index)
{
    // auto chunkIndex = indexToStationChunk(index);
    // if(chunkIndex.first != nullptr) {
    //     chunkIndex.first->addCalibration(chunkIndex.second);

    //     QModelIndex dataChangeIndex = this->index(index);

    //     emit dataChanged(dataChangeIndex, dataChangeIndex, QVector<int>() << ShotCalibrationRole);
    // }
}

/**
 * @brief cwSurveyEditorModel::updateIndexToChunk
 *
 * This clears the current m_indexToChunk and updates it with a new lookup
 */
void cwSurveyEditorModel::updateIndexToChunk()
{
   // m_indexToChunk.clear();

   // if(m_trip.isNull()) {
   //     return;
   // }

   // int stationCount = m_skipRowOffset; //1 enables the tile bar to be displayed for the first chunk in the view
   // foreach(cwSurveyChunk* chunk, m_trip->chunks()) {
   //     int begin = stationCount;
   //     int end = begin + chunk->stationCount() - m_skipRowOffset;
   //     // qDebug() << "Insert:" << chunk << begin << end;
   //     m_indexToChunk.insert(Range(begin, end), chunk);
   //     stationCount += chunk->stationCount() + m_skipRowOffset;
   // }
}

int cwSurveyEditorModel::chunkIndexToRow(const cwSurveyChunk *chunk, int chunkIndex) const
{
    // Compute base model row for this chunk.
    int baseRow = 0;
    const QList<cwSurveyChunk*> allChunks = m_trip->chunks();
    for (cwSurveyChunk* c : allChunks) {
        if (c == chunk) {
            baseRow += m_skipRowOffset;
            break;
        }
        baseRow += c->stationCount() + m_skipRowOffset;
    }

    Q_ASSERT(chunkIndex >= 0 && chunkIndex < chunk->stationCount());
    return baseRow + chunkIndex;
}

QModelIndex cwSurveyEditorModel::chunkIndexToModelIndex(const cwSurveyChunk *chunk, int chunkIndex) const
{

    return index(chunkIndexToRow(chunk, chunkIndex));
}

cwSurveyEditorModel::Roles cwSurveyEditorModel::chunkRoleToModelRole(cwSurveyChunk::DataRole chunkRole)
{
    switch(chunkRole) {
    case cwSurveyChunk::StationNameRole:
        return StationNameRole;
    case cwSurveyChunk::StationLeftRole:
        return StationLeftRole;
    case cwSurveyChunk::StationRightRole:
        return StationRightRole;
    case cwSurveyChunk::StationUpRole:
        return StationUpRole;
    case cwSurveyChunk::StationDownRole:
        return StationDownRole;
    case cwSurveyChunk::ShotDistanceRole:
        return ShotDistanceRole;
    case cwSurveyChunk::ShotDistanceIncludedRole:
    case cwSurveyChunk::ShotCompassRole:
        return ShotCompassRole;
    case cwSurveyChunk::ShotBackCompassRole:
        return ShotBackCompassRole;
    case cwSurveyChunk::ShotClinoRole:
        return ShotClinoRole;
    case cwSurveyChunk::ShotBackClinoRole:
        return ShotBackClinoRole;
    }
}

/**
 * @brief cwSurveyEditorModel::modelIndexToStationChunk
 * @param index - The index from the model
 * @return The station index in the chunk and the chunk at index
 */
cwSurveyEditorModel::ChunkIndex cwSurveyEditorModel::modelIndexToStationChunk(const QModelIndex& index) const
{
    return indexToStationChunk(index.row());
}

/**
 * @brief cwSurveyEditorModel::indexToStationChunk
 * @param index - The row in model
 * @return The station index in teh chunk and teh cuhnk at index
 */
cwSurveyEditorModel::ChunkIndex cwSurveyEditorModel::indexToStationChunk(int index) const
{
    auto chunks = m_trip->chunks();
    for(const auto chunk : std::as_const(chunks)) {
        auto diff = index - (chunk->stationCount() + m_skipRowOffset);
        if(diff < 0) {
            index = index - m_skipRowOffset;
            if(index < 0) {
                //Skip row
                return {nullptr, -1};
            }
            return {chunk, index};
        }
        index = diff;
    }

    //Nothing found
    return {nullptr, -1};


    // auto iter = m_indexToChunk.lowerBound(Range(index));

    // if(iter != m_indexToChunk.end() && iter.key().low() <= index) {
    //     int realIndex = index - iter.key().low();
    //     cwSurveyChunk* chunk = iter.value();
    //     Q_ASSERT(realIndex < chunk->stationCount());
    //     Q_ASSERT(realIndex >= 0);
    //     return {chunk, realIndex};
    // }

    // return {nullptr, -1};
}
