//Our includes
#include "cwSurveyEditorModel.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwTripCalibration.h"

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


                connect(chunk, &cwSurveyChunk::dataChanged, this,
                        [this, chunk](cwSurveyChunk::DataRole role, int stationIndex) {
                            auto rowType = toRowType(role);
                            auto modelIndex = toModelIndex(rowType, chunk, stationIndex);
                            Q_ASSERT(modelIndex.isValid());
                            emit dataChanged(modelIndex, modelIndex, {toModelRole(role)});
                        });

                // Stations added.
                connect(chunk, &cwSurveyChunk::added, this,
                        [this, chunk](int stationBegin, int stationEnd, int shotBegin, int shotEnd) {
                            int first = std::min(toRow(StationRow, chunk, stationBegin),
                                                 toRow(ShotRow, chunk, shotBegin));
                            int last = std::max(toRow(StationRow, chunk, stationEnd),
                                               toRow(ShotRow, chunk, shotEnd));
                            beginInsertRows(QModelIndex(), first, last);
                            endInsertRows();
                        });
                // Stations removed.
                connect(chunk, &cwSurveyChunk::aboutToRemove, this,
                        [this, chunk](int stationBegin, int stationEnd, int shotBegin, int shotEnd) {
                            int first = std::min(toRow(StationRow, chunk, stationBegin),
                                                 toRow(ShotRow, chunk, shotBegin));
                            int last = std::max(toRow(StationRow, chunk, stationEnd),
                                                toRow(ShotRow, chunk, shotEnd));
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

        endResetModel();

        emit tripChanged();
    }
}



QVariant cwSurveyEditorModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid()) {
        return QVariant();
    }


    auto chunkIndex = toChunkIndex(index);

    switch(role) {
    case ChunkRole:
        return QVariant::fromValue(chunkIndex.chunk);
    case RowTypeRole:
        return chunkIndex.type;
    case IndexInChunkRole:
        return chunkIndex.index;
    default:
        break;
    }

    auto titleData = [index, role](const ChunkIndex& chunkIndex)->QVariant {
        if(role == RowTypeRole) {
            return TitleRow;
        }
        return QVariant();
    };

    auto stationData = [index, role](const ChunkIndex& chunkIndex)->QVariant {
        const auto chunk = chunkIndex.chunk;
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
        default:
            return QVariant();
        }
    };

    auto shotData = [index, role](const ChunkIndex& chunkIndex)->QVariant {
        const auto chunk = chunkIndex.chunk;
        int shotIndex = chunkIndex.index;
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
        default:
            return QVariant();
        }
    };

    switch(chunkIndex.type) {
    case TitleRow:
        return titleData(chunkIndex);
    case StationRow:
        return stationData(chunkIndex);
    case ShotRow:
        return shotData(chunkIndex);
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
    roles.insert(RowTypeRole, "rowType");
    // roles.insert(StationVisibleRole, "stationVisible");
    // roles.insert(ShotVisibleRole, "shotVisible");
    // roles.insert(TitleVisibleRole, "titleVisible");
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

cwSurveyEditorModel::RowType cwSurveyEditorModel::toRowType(cwSurveyChunk::DataRole chunkRole)
{
    switch(chunkRole) {
    case cwSurveyChunk::StationNameRole:
    case cwSurveyChunk::StationLeftRole:
    case cwSurveyChunk::StationRightRole:
    case cwSurveyChunk::StationUpRole:
    case cwSurveyChunk::StationDownRole:
        return StationRow;
    case cwSurveyChunk::ShotDistanceRole:
    case cwSurveyChunk::ShotDistanceIncludedRole:
    case cwSurveyChunk::ShotCompassRole:
    case cwSurveyChunk::ShotBackCompassRole:
    case cwSurveyChunk::ShotClinoRole:
    case cwSurveyChunk::ShotBackClinoRole:
        return ShotRow;
    }
}

int cwSurveyEditorModel::toRow(RowType type, const cwSurveyChunk *chunk, int chunkIndex) const
{
    // Compute base model row for this chunk.
    int baseRow = 0;
    const QList<cwSurveyChunk*> allChunks = m_trip->chunks();
    for (cwSurveyChunk* current : allChunks) {
        if (current == chunk) {
            break;
        }
        baseRow += current->stationCount() + current->shotCount() + m_titleRowOffset;
    }

    auto toIndex = [=]()->int {
        switch(type) {
        case TitleRow:
            return baseRow;
        case StationRow:
            return baseRow + chunkIndex * 2 + m_titleRowOffset; //Alterate between the station and shot
        case ShotRow:
            return baseRow + chunkIndex * 2 + 1 + m_titleRowOffset;
        }
    };

    Q_ASSERT(toIndex() >= 0);
    Q_ASSERT(toIndex() < rowCount());
    return toIndex();
}

QModelIndex cwSurveyEditorModel::toModelIndex(RowType type, const cwSurveyChunk *chunk, int chunkIndex) const
{

    return index(toRow(type, chunk, chunkIndex));
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
 * @brief cwSurveyEditorModel::toChunkIndex
 * @param index - The index from the model
 * @return The station index in the chunk and the chunk at index
 */
cwSurveyEditorModel::ChunkIndex cwSurveyEditorModel::toChunkIndex(const QModelIndex& index) const
{
    return toChunkIndex(index.row());
}

/**
 * @brief cwSurveyEditorModel::toChunkIndex
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
cwSurveyEditorModel::ChunkIndex cwSurveyEditorModel::toChunkIndex(int index) const
{
    auto chunks = m_trip->chunks();
    for(const auto chunk : std::as_const(chunks)) {
        auto diff = index - (chunk->stationCount() + chunk->shotCount() + m_titleRowOffset);
        if(diff < 0) {
            index = index - m_titleRowOffset;
            if(index < 0) {
                //Title row
                return {chunk, TitleRow, -1};
            }

            if(index % 2 == 0) {
                //Is a station
                int stationIndex = index / 2;
                Q_ASSERT(stationIndex >= 0);
                Q_ASSERT(stationIndex < chunk->stationCount());
                return {chunk, StationRow, stationIndex};
            } else {
                //Is a shot
                int shotIndex = index / 2;
                Q_ASSERT(shotIndex >= 0);
                Q_ASSERT(shotIndex < chunk->shotCount());
                return {chunk, ShotRow, shotIndex};
            }
        }
        index = diff;
    }

    //Nothing found
    return {nullptr, TitleRow, -1};
}
