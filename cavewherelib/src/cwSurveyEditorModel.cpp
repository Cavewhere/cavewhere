//Our includes
#include "cwSurveyEditorModel.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwTripCalibration.h"
#include "cwSurveyEditorBoxData.h"

cwSurveyEditorModel::cwSurveyEditorModel()
{

}

void cwSurveyEditorModel::setTrip(cwTrip* trip) {
    if(m_trip != trip) {
        beginResetModel();
        m_trip = trip;

        if(m_trip) {
            // Lambda to connect signals for a cwSurveyChunk.
            auto connectChunk = [this](cwSurveyChunk* chunk) {
                // Data changes in the chunk.

                auto chunkDataChange = [this, chunk](cwSurveyChunk::DataRole role, int chunkIndex) {
                    auto rowType = toRowType(role);
                    auto modelIndex = toModelIndex({chunk, chunkIndex, rowType});
                    Q_ASSERT(modelIndex.isValid());
                    emit dataChanged(modelIndex, modelIndex, {toModelRole(role)});
                };

                connect(chunk, &cwSurveyChunk::dataChanged, this, chunkDataChange);
                connect(chunk, &cwSurveyChunk::errorsChanged, this, chunkDataChange);

                // Stations added.
                connect(chunk, &cwSurveyChunk::added, this,
                        [this, chunk](int stationBegin, int stationEnd, int shotBegin, int shotEnd) {
                            int first = std::min(toModelRow({chunk, stationBegin, cwSurveyEditorRowIndex::StationRow} ),
                                                 toModelRow({chunk, shotBegin, cwSurveyEditorRowIndex::ShotRow}));
                            int last = std::max(toModelRow({chunk, stationEnd, cwSurveyEditorRowIndex::StationRow}),
                                                toModelRow({chunk, shotEnd, cwSurveyEditorRowIndex::ShotRow}));
                            beginInsertRows(QModelIndex(), first, last);
                            endInsertRows();
                        });
                // Stations removed.
                connect(chunk, &cwSurveyChunk::aboutToRemove, this,
                        [this, chunk](int stationBegin, int stationEnd, int shotBegin, int shotEnd) {
                            int first = std::min(toModelRow({chunk, stationBegin, cwSurveyEditorRowIndex::StationRow}),
                                                 toModelRow({chunk, shotBegin, cwSurveyEditorRowIndex::ShotRow}));
                            int last = std::max(toModelRow({chunk, stationEnd, cwSurveyEditorRowIndex::StationRow}),
                                                toModelRow({chunk, shotEnd, cwSurveyEditorRowIndex::ShotRow}));
                            beginRemoveRows(QModelIndex(), first, last);
                });

                connect(chunk, &cwSurveyChunk::removed, this,
                        [this, chunk](int stationBegin, int stationEnd, int shotBegin, int shotEnd) {
                            endRemoveRows();
                        });
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
                    const auto chunk = allChunks.at(i);
                    modelRow += chunk->stationCount() + chunk->shotCount()  + m_titleRowOffset;
                }
                int totalNewRows = 0;
                for (int i = chunkBegin; i <= chunkEnd && i < allChunks.size(); ++i) {
                    const auto chunk = allChunks.at(i);
                    totalNewRows += chunk->stationCount() + chunk->shotCount() + m_titleRowOffset;
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

                        const auto lastIndex = m_trip->chunkCount() - 1;
                        if(end == lastIndex) {
                            emit lastChunkAdded();
                        }

                        // qDebug() << "chunk inserted editor model:" << begin << end;
                    });

            connect(m_trip, &cwTrip::chunksAboutToBeRemoved, this,
                    [this, disconnectChunk, modelRowRange](int begin, int end) {
                        auto modelRange = modelRowRange(begin, end);
                        beginRemoveRows(QModelIndex(), modelRange.begin, modelRange.end);

                        const QList<cwSurveyChunk*> allChunks = m_trip->chunks();
                        for (int i = begin; i <= end && i < allChunks.size(); ++i) {
                            auto chunk = allChunks.at(i);
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

        endResetModel();

        emit tripChanged();
    }
}



QVariant cwSurveyEditorModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid()) {
        return QVariant();
    }


    const auto rowIndex = toRowIndex(index);

    if(role == RowIndexRole) {
        return QVariant::fromValue(rowIndex);
    }

    // switch(role) {
    // case ChunkRole:
    //     return QVariant::fromValue(rowIndex.chunk);
    // case RowTypeRole:
    //     return rowIndex.type;
    // case IndexInChunkRole:
    //     qDebug() << "IndexInChunkRole:" << rowIndex.index << index << role;
    //     return rowIndex.index;
    // default:
    //     break;
    // }

    auto titleData = [index, role](const cwSurveyEditorRowIndex& chunkIndex)->QVariant {
        return QVariant();
    };

    auto data = [rowIndex](cwSurveyChunk::DataRole dataRole, auto dataFn) {
        return QVariant::fromValue(cwSurveyEditorBoxData(
            dataFn(),
            rowIndex,
            dataRole,
            rowIndex.chunk()->errorsAt(rowIndex.indexInChunk(), dataRole)
            ));
    };

    auto stationData = [index, role, data](const cwSurveyEditorRowIndex& chunkIndex)->QVariant {
        const auto chunk = chunkIndex.chunk();
        const int stationIndex = chunkIndex.indexInChunk();
        switch(role) {
        case StationNameRole:
            return data(cwSurveyChunk::StationNameRole, [&]() { return cwReading(chunk->station(stationIndex).name()); });
        case StationLeftRole:
            return data(cwSurveyChunk::StationLeftRole, [&]() { return chunk->station(stationIndex).left(); } );
        case StationRightRole:
            return data(cwSurveyChunk::StationRightRole, [&]() { return chunk->station(stationIndex).right(); });
        case StationUpRole:
            return data(cwSurveyChunk::StationUpRole, [&]() { return chunk->station(stationIndex).up(); });
        case StationDownRole:
            return data(cwSurveyChunk::StationDownRole, [&]() { return chunk->station(stationIndex).down(); });
        default:
            return QVariant();
        }
    };

    auto shotData = [index, role, data](const cwSurveyEditorRowIndex& chunkIndex)->QVariant {
        const auto chunk = chunkIndex.chunk();
        const int shotIndex = chunkIndex.indexInChunk();
        switch(role) {
        case ShotDistanceRole:
            return data(cwSurveyChunk::ShotDistanceRole, [&]() { return chunk->shot(shotIndex).distance(); });
        case ShotCompassRole:
            return data(cwSurveyChunk::ShotCompassRole, [&]() { return chunk->shot(shotIndex).compass(); });
        case ShotBackCompassRole:
            return data(cwSurveyChunk::ShotBackCompassRole, [&]() { return chunk->shot(shotIndex).backCompass(); });
        case ShotClinoRole:
            return data(cwSurveyChunk::ShotClinoRole, [&]() { return chunk->shot(shotIndex).clino(); });
        case ShotBackClinoRole:
            return data(cwSurveyChunk::ShotBackClinoRole, [&]() { return chunk->shot(shotIndex).backClino(); });
        case ShotCalibrationRole: {
            cwTripCalibration* calibration = chunk->calibrations().value(shotIndex);
            return QVariant::fromValue(calibration);
        }
        default:
            return QVariant();
        }
    };

    switch(rowIndex.rowType()) {
    case cwSurveyEditorRowIndex::TitleRow:
        return titleData(rowIndex);
    case cwSurveyEditorRowIndex::StationRow:
        return stationData(rowIndex);
    case cwSurveyEditorRowIndex::ShotRow:
        return shotData(rowIndex);
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
        count += chunk->stationCount() + chunk->shotCount() + m_titleRowOffset;
    }
    return count;
}

/**
 * @brief cwSurveyEditorModel::roleNames
 * @return
 */
QHash<int, QByteArray> cwSurveyEditorModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(RowIndexRole, "rowIndex");
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
    // roles.insert(ChunkRole, "chunk");
    // roles.insert(RowTypeRole, "rowType");
    // roles.insert(StationVisibleRole, "stationVisible");
    // roles.insert(ShotVisibleRole, "shotVisible");
    // roles.insert(TitleVisibleRole, "titleVisible");
    // roles.insert(IndexInChunkRole, "indexInChunk");
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

int cwSurveyEditorModel::toModelRow(const cwSurveyEditorRowIndex &rowIndex) const
{
    // Compute base model row for this chunk.
    int baseRow = 0;
    const QList<cwSurveyChunk*> allChunks = m_trip->chunks();
    for (cwSurveyChunk* current : allChunks) {
        if (current == rowIndex.chunk()) {
            break;
        }
        baseRow += current->stationCount() + current->shotCount() + m_titleRowOffset;
    }

    auto toIndex = [&]()->int {
        switch(rowIndex.rowType()) {
        case cwSurveyEditorRowIndex::TitleRow:
            return baseRow;
        case cwSurveyEditorRowIndex::StationRow:
            return baseRow + rowIndex.indexInChunk() * 2 + m_titleRowOffset; //Alterate between the station and shot
        case cwSurveyEditorRowIndex::ShotRow:
            return baseRow + rowIndex.indexInChunk() * 2 + 1 + m_titleRowOffset;
        }
    };

    Q_ASSERT(toIndex() >= 0);
    Q_ASSERT(toIndex() < rowCount());
    return toIndex();
}

cwSurveyEditorRowIndex cwSurveyEditorModel::rowIndex(cwSurveyChunk *chunk, int chunkIndex, cwSurveyEditorRowIndex::RowType type) const
{
    return cwSurveyEditorRowIndex(chunk, chunkIndex, type);
}

cwSurveyEditorRowIndex::RowType cwSurveyEditorModel::toRowType(cwSurveyChunk::DataRole chunkDataRole)
{
    switch(chunkDataRole) {
    case cwSurveyChunk::StationNameRole:
    case cwSurveyChunk::StationLeftRole:
    case cwSurveyChunk::StationRightRole:
    case cwSurveyChunk::StationUpRole:
    case cwSurveyChunk::StationDownRole:
        return cwSurveyEditorRowIndex::StationRow;
    case cwSurveyChunk::ShotDistanceRole:
    case cwSurveyChunk::ShotDistanceIncludedRole:
    case cwSurveyChunk::ShotCompassRole:
    case cwSurveyChunk::ShotBackCompassRole:
    case cwSurveyChunk::ShotClinoRole:
    case cwSurveyChunk::ShotBackClinoRole:
        return cwSurveyEditorRowIndex::ShotRow;
    }
}

// int cwSurveyEditorModel::toRow(RowType type, const cwSurveyChunk *chunk, int chunkIndex) const
// {

// }

// cwSurveyEditorRowIndex cwSurveyEditorModel::boxIndex(RowType type,
//                                                      cwSurveyChunk *chunk,
//                                                      int chunkIndex,
//                                                      cwSurveyChunk::DataRole dataRole)
// {
//     return cwSurveyEditorRowIndex(type, chunk, chunkIndex, dataRole);
// }

// cwSurveyEditorRowIndex cwSurveyEditorModel::boxIndex(int row, cwSurveyChunk::DataRole dataRole) const
// {
//     auto chunkIndex = toRowIndex(row);
//     return cwSurveyEditorRowIndex(chunkIndex.type,
//                                   chunkIndex.chunk,
//                                   chunkIndex.index,
//                                   dataRole
//                                   );
// }

QModelIndex cwSurveyEditorModel::toModelIndex(const cwSurveyEditorRowIndex &rowIndex) const
{

    return index(toModelRow(rowIndex));
}

cwSurveyEditorModel::Role cwSurveyEditorModel::toModelRole(cwSurveyChunk::DataRole chunkRole)
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
 * @brief cwSurveyEditorModel::toRowIndex
 * @param index - The index from the model
 * @return The station index in the chunk and the chunk at index
 */
cwSurveyEditorRowIndex cwSurveyEditorModel::toRowIndex(const QModelIndex& index) const
{
    return toRowIndex(index.row());
}

/**
 * @brief cwSurveyEditorModel::toRowIndex
 * @param index - The row in model
 * @return The station index in teh chunk and teh cuhnk at index
 *
 * The model alternates row data.
 * 0. Title row
 * 1. station row
 * 2. shot row
 * 3. station row
 * 4. ...
 */
cwSurveyEditorRowIndex cwSurveyEditorModel::toRowIndex(int index) const
{
    auto chunks = m_trip->chunks();
    for(const auto chunk : std::as_const(chunks)) {
        auto diff = index - (chunk->stationCount() + chunk->shotCount() + m_titleRowOffset);
        if(diff < 0) {
            index = index - m_titleRowOffset;
            if(index < 0) {
                //Title row
                return {chunk, -1, cwSurveyEditorRowIndex::TitleRow};
            }

            if(index % 2 == 0) {
                //Is a station
                int stationIndex = index / 2;
                Q_ASSERT(stationIndex >= 0);
                Q_ASSERT(stationIndex < chunk->stationCount());
                return {chunk, stationIndex, cwSurveyEditorRowIndex::StationRow};
            } else {
                //Is a shot
                int shotIndex = index / 2;
                Q_ASSERT(shotIndex >= 0);
                Q_ASSERT(shotIndex < chunk->shotCount());
                return {chunk, shotIndex, cwSurveyEditorRowIndex::ShotRow};
            }
        }
        index = diff;
    }

    //Nothing found
    return cwSurveyEditorRowIndex();
}
