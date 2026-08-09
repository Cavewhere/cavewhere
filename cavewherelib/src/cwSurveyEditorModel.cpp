//Our includes
#include "cwSurveyEditorModel.h"
#include "cwTrip.h"
#include "cwCave.h"
#include "cwFixStationModel.h"
#include "cwSurveyChunk.h"
#include "cwTripCalibration.h"
#include "cwSurveyEditorBoxData.h"
#include "cwDebug.h"
#include "cwReading.h"
#include "cwShotMeasurement.h"

#include <QDebug>

//Std includes
#include <limits>

namespace {

/**
 * Most cells in the editor stand for a reading the chunk stores, and those are
 * the only ones a write can reach. The Splays cell is a handle on the station's
 * cluster instead, so it names no reading to write to.
 */
bool isReadingCell(cwSurveyChunk::DataRole role)
{
    return role != cwSurveyChunk::StationSplaysRole;
}

}

cwSurveyEditorModel::cwSurveyEditorModel()
{
    connect(this, &QAbstractItemModel::rowsInserted, this, [this]() {
        syncFocusedCellSignals();
    });
    connect(this, &QAbstractItemModel::rowsRemoved, this, [this]() {
        syncFocusedCellSignals();
    });
    connect(this, &QAbstractItemModel::modelReset, this, [this]() {
        syncFocusedCellSignals();
    });
}

void cwSurveyEditorModel::disconnectChunkSignals(cwSurveyChunk* chunk)
{
    if(chunk == nullptr) {
        return;
    }
    disconnect(chunk, nullptr, this, nullptr);
}

void cwSurveyEditorModel::connectChunkSignals(cwSurveyChunk* chunk)
{
    if(chunk == nullptr) {
        return;
    }

    auto emitDataChangedForChunk = [this](cwSurveyChunk* changedChunk, int firstRow) {
        if(!changedChunk) {
            return;
        }

        int baseRow = toModelRow({changedChunk, -1, cwSurveyEditorRowIndex::TitleRow});
        int lastRow = baseRow + chunkRowCount(changedChunk) - 1;
        if(firstRow <= lastRow) {
            emit dataChanged(index(firstRow), index(lastRow));
        }
    };

    auto chunkDataChange = [this, chunk](cwSurveyChunk::DataRole role, int chunkIndex) {
        auto rowType = toRowType(role);
        auto modelIndex = toModelIndex({chunk, chunkIndex, rowType});
        if(!modelIndex.isValid()) {
            syncVirtualRows(chunk);
            return;
        }
        emit dataChanged(modelIndex, modelIndex, changedRolesFor(role));

        //An open cluster labels each of its rows with the station's name, so a
        //rename has to reach the rows hanging below the station row too
        if(role == cwSurveyChunk::StationNameRole) {
            const int splayRows = splayRowCount(chunk, chunkIndex);
            if(splayRows > 0) {
                emit dataChanged(index(modelIndex.row() + 1),
                                 index(modelIndex.row() + splayRows),
                                 {SplayStationNameRole});
            }
        }

        syncVirtualRows(chunk);
    };

    connect(chunk, &cwSurveyChunk::dataChanged, this, chunkDataChange);
    connect(chunk, &cwSurveyChunk::errorsChanged, this, chunkDataChange);

    connect(chunk, &cwSurveyChunk::stationSplaysChanged, this, [this, chunk](int stationIndex) {
        reconcileSplayRows(chunk, stationIndex);
    });

    connect(chunk, &cwSurveyChunk::added, this,
            [this, chunk, emitDataChangedForChunk](int stationBegin, int stationEnd, int shotBegin, int shotEnd) {
                shiftExpandedSplays(chunk, stationBegin, stationEnd - stationBegin + 1);
                int first = std::min(toModelRow({chunk, stationBegin, cwSurveyEditorRowIndex::StationRow}),
                                     toModelRow({chunk, shotBegin, cwSurveyEditorRowIndex::ShotRow}));
                int last = std::max(toModelRow({chunk, stationEnd, cwSurveyEditorRowIndex::StationRow}),
                                    toModelRow({chunk, shotEnd, cwSurveyEditorRowIndex::ShotRow}));
                beginInsertRows(QModelIndex(), first, last);
                endInsertRows();
                emitDataChangedForChunk(chunk, last + 1);
                if(rowCount() > 0 && first >= 0 && first < rowCount()) {
                    emit dataChanged(index(first), index(rowCount() - 1));
                }
            });

    connect(chunk, &cwSurveyChunk::aboutToRemove, this,
            [this, chunk](int stationBegin, int stationEnd, int shotBegin, int shotEnd) {
                //A cluster on a station that's leaving has to close on its own,
                //so its rows go away as a removal the view can follow
                for(int stationIndex = stationEnd; stationIndex >= stationBegin; --stationIndex) {
                    collapseSplays(chunk, stationIndex);
                }

                int first = std::min(toModelRow({chunk, stationBegin, cwSurveyEditorRowIndex::StationRow}),
                                     toModelRow({chunk, shotBegin, cwSurveyEditorRowIndex::ShotRow}));
                int last = std::max(toModelRow({chunk, stationEnd, cwSurveyEditorRowIndex::StationRow}),
                                    toModelRow({chunk, shotEnd, cwSurveyEditorRowIndex::ShotRow}));
                Q_ASSERT(m_removeToken.chunk == nullptr);
                m_removeToken = {chunk, first};
                beginRemoveRows(QModelIndex(), first, last);
            });

    connect(chunk, &cwSurveyChunk::removed, this,
            [this, chunk](int stationBegin, int stationEnd, int, int) {
                shiftExpandedSplays(chunk, stationBegin, stationBegin - stationEnd - 1);
                endRemoveRows();
                Q_ASSERT(m_removeToken.chunk != nullptr);
                syncVirtualRows(chunk);
                if(rowCount() > 0
                        && m_removeToken.firstIndex >= 0
                        && m_removeToken.firstIndex < rowCount()) {
                    emit dataChanged(index(m_removeToken.firstIndex), index(rowCount() - 1));
                }
                m_removeToken = {};
            });
}

/**
 * Points StationFixedRole at the fixes of whichever cave the current trip
 * belongs to. Re-run whenever the trip or its cave changes — a trip can be
 * reparented, and a trip loaded before its cave is attached has no fixes yet.
 */
void cwSurveyEditorModel::syncFixStationSignals()
{
    auto cave = m_trip ? m_trip->parentCave() : nullptr;
    auto fixStations = cave ? cave->fixStations() : nullptr;

    if(m_fixStations == fixStations) {
        return;
    }

    if(m_fixStations) {
        disconnect(m_fixStations, &cwFixStationModel::fixedStationsChanged,
                   this, &cwSurveyEditorModel::invalidateStationFixed);
    }

    m_fixStations = fixStations;

    if(m_fixStations) {
        connect(m_fixStations, &cwFixStationModel::fixedStationsChanged,
                this, &cwSurveyEditorModel::invalidateStationFixed);
    }

    invalidateStationFixed();
}

/**
 * A fix was added, removed or renamed, so any station row could have changed
 * its answer. Which ones isn't knowable without re-reading every name, and the
 * role is a single lookup, so the whole model is invalidated instead.
 */
void cwSurveyEditorModel::invalidateStationFixed()
{
    const int rows = rowCount();
    if(rows > 0) {
        emit dataChanged(index(0), index(rows - 1), {StationFixedRole});
    }
}

void cwSurveyEditorModel::setTrip(cwTrip* trip) {
    if(m_trip != trip) {
        beginResetModel();
        m_focusedChunk = nullptr;
        m_virtualRowsVisibleChunk = nullptr;
        m_focusedRowIndex = QPersistentModelIndex();
        m_focusedDataRole = static_cast<cwSurveyChunk::DataRole>(-1);
        m_expandedSplays.clear();

        if(m_trip) {
            const auto chunks = m_trip->chunks();
            for(auto chunk : chunks) {
                disconnectChunkSignals(chunk);
            }
            disconnect(m_trip, nullptr, this, nullptr);
        }

        m_trip = trip;

        if(m_trip) {
            struct Range {
                int begin;
                int end;
            };

            auto modelRowRange = [this](int chunkBegin, int chunkEnd)->Range {
                int modelRow = 0;
                const QList<cwSurveyChunk*> allChunks = m_trip->chunks();
                for (int i = 0; i < chunkBegin && i < allChunks.size(); ++i) {
                    const auto chunk = allChunks.at(i);
                    modelRow += chunkRowCount(chunk);
                }
                int totalNewRows = 0;
                for (int i = chunkBegin; i <= chunkEnd && i < allChunks.size(); ++i) {
                    const auto chunk = allChunks.at(i);
                    totalNewRows += chunkRowCount(chunk);
                }
                return {modelRow, modelRow + totalNewRows - 1};
            };

            // Map cwTrip chunk-level changes to model row changes
            connect(m_trip, &cwTrip::chunksInserted, this,
                    [this, modelRowRange](int begin, int end) {
                        // Calculate the model row where insertion begins.
                        auto modelRange = modelRowRange(begin, end);

                        const QList<cwSurveyChunk*> allChunks = m_trip->chunks();
                        for (int i = begin; i <= end && i < allChunks.size(); ++i) {
                            connectChunkSignals(allChunks.at(i));
                        }

                        beginInsertRows(QModelIndex(), modelRange.begin, modelRange.end);
                        endInsertRows();

                        const auto lastIndex = m_trip->chunkCount() - 1;
                        if(end == lastIndex) {
                            emit lastChunkAdded();
                        }

                    });

            connect(m_trip, &cwTrip::chunksAboutToBeRemoved, this,
                    [this, modelRowRange](int begin, int end) {
                        auto modelRange = modelRowRange(begin, end);
                        beginRemoveRows(QModelIndex(), modelRange.begin, modelRange.end);

                        const QList<cwSurveyChunk*> allChunks = m_trip->chunks();
                        for (int i = begin; i <= end && i < allChunks.size(); ++i) {
                            auto chunk = allChunks.at(i);
                            if(chunk == m_focusedChunk) {
                                m_virtualRowsVisibleChunk = nullptr;
                            }
                            m_expandedSplays.remove(chunk);
                            disconnectChunkSignals(allChunks.at(i));
                        }
                    });

            connect(m_trip, &cwTrip::chunksRemoved, this,
                    [this](int begin, int end) {
                        Q_UNUSED(begin);
                        Q_UNUSED(end);
                        endRemoveRows();
                    });

            connect(m_trip, &cwTrip::parentCaveChanged, this,
                    &cwSurveyEditorModel::syncFixStationSignals);

            // Connect signals for all existing chunks.
            const auto chunks = m_trip->chunks();
            for (cwSurveyChunk* chunk : chunks) {
                connectChunkSignals(chunk);
            }
        }

        endResetModel();
        syncFocusedCellSignals();
        syncFixStationSignals();

        emit tripChanged();
    }
}

void cwSurveyEditorModel::setFocusedChunk(cwSurveyChunk* chunk)
{
    if(m_focusedChunk == chunk) {
        return;
    }

    auto firstAffectedRowForChunk = [this](cwSurveyChunk* candidate) {
        if(candidate == nullptr) {
            return rowCount();
        }
        const int base = toModelRow({candidate, -1, cwSurveyEditorRowIndex::TitleRow});
        return base >= 0 ? base : rowCount();
    };

    int firstAffectedRow = std::min(firstAffectedRowForChunk(m_focusedChunk),
                                    firstAffectedRowForChunk(chunk));
    firstAffectedRow = std::min(firstAffectedRow,
                                firstAffectedRowForChunk(m_virtualRowsVisibleChunk));

    auto oldFocusedChunk = m_focusedChunk;

    if(oldFocusedChunk) {
        trim(oldFocusedChunk, FullTrim);
    }

    if(m_virtualRowsVisibleChunk) {
        const int first = firstVirtualRow(m_virtualRowsVisibleChunk);
        const int last = first + 1;
        beginRemoveRows(QModelIndex(), first, last);
        m_virtualRowsVisibleChunk = nullptr;
        endRemoveRows();
    }

    m_focusedChunk = chunk;

    if(m_focusedChunk) {
        connect(m_focusedChunk, &QObject::destroyed, this, [this]() {
            m_focusedChunk = nullptr;
            m_virtualRowsVisibleChunk = nullptr;
            beginResetModel();
            endResetModel();
        });
    }

    syncVirtualRows(m_focusedChunk);

    if(rowCount() > 0 && firstAffectedRow < rowCount()) {
        emit dataChanged(index(firstAffectedRow, 0), index(rowCount() - 1, 0));
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

    if(role == RowTypeRole) {
        return static_cast<int>(rowIndex.rowType());
    }

    if(role == IndexInChunkRole) {
        return rowIndex.indexInChunk();
    }

    if(role == ChunkRole) {
        return QVariant::fromValue(rowIndex.chunk());
    }

    if(role == IsVirtualRole) {
        if(rowIndex.chunk() == nullptr) {
            return false;
        }

        const bool hasVirtualRows = hasVirtualTrailingStationShot(rowIndex.chunk());
        if(!hasVirtualRows) {
            return false;
        }

        if(rowIndex.rowType() == cwSurveyEditorRowIndex::StationRow) {
            return rowIndex.indexInChunk() == rowIndex.chunk()->stationCount();
        }

        if(rowIndex.rowType() == cwSurveyEditorRowIndex::ShotRow) {
            return rowIndex.indexInChunk() == rowIndex.chunk()->shotCount();
        }

        return false;
    }

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

    auto stationData = [this, index, role, data](const cwSurveyEditorRowIndex& chunkIndex)->QVariant {
        const auto chunk = chunkIndex.chunk();
        const int stationIndex = chunkIndex.indexInChunk();
        if(stationIndex == chunk->stationCount()) {
            switch(role) {
            case StationFixedRole:
                //The trailing virtual row has no name, so nothing can anchor it
                return false;
            case StationSplaysExpandedRole:
                return false;
            case StationSplayCountRole:
                return 0;
            case StationNameRole:
                return data(cwSurveyChunk::StationNameRole, [&]() { return cwReading(QString()); });
            case StationLeftRole:
                return data(cwSurveyChunk::StationLeftRole, [&]() { return cwReading(QString()); });
            case StationRightRole:
                return data(cwSurveyChunk::StationRightRole, [&]() { return cwReading(QString()); });
            case StationUpRole:
                return data(cwSurveyChunk::StationUpRole, [&]() { return cwReading(QString()); });
            case StationDownRole:
                return data(cwSurveyChunk::StationDownRole, [&]() { return cwReading(QString()); });
            default:
                return QVariant();
            }
        }
        switch(role) {
        case StationFixedRole:
            return m_fixStations != nullptr
                    && m_fixStations->isFixed(chunk->station(stationIndex).name());
        case StationSplayCountRole:
            return chunk->stationSplayCount(stationIndex);
        case StationSplaysExpandedRole:
            return splayRowCount(chunk, stationIndex) > 0;
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

    auto shotData = [this, index, role, data](const cwSurveyEditorRowIndex& chunkIndex)->QVariant {
        const auto chunk = chunkIndex.chunk();
        const int shotIndex = chunkIndex.indexInChunk();

        if(role == StationSplaysExpandedRole) {
            //Shot i hangs under station i, so it answers for that station's cluster
            return splayRowCount(chunk, shotIndex) > 0;
        }

        if(shotIndex == chunk->shotCount()) {
            if(role == ShotCalibrationRole) {
                return QVariant::fromValue(static_cast<cwTripCalibration*>(nullptr));
            }
            switch(role) {
            case ShotDistanceRole:
                return data(cwSurveyChunk::ShotDistanceRole, [&]() { return cwReading(QString()); });
            case ShotDistanceIncludedRole:
                return true;
            case ShotCompassRole:
                return data(cwSurveyChunk::ShotCompassRole, [&]() { return cwReading(QString()); });
            case ShotBackCompassRole:
                return data(cwSurveyChunk::ShotBackCompassRole, [&]() { return cwReading(QString()); });
            case ShotClinoRole:
                return data(cwSurveyChunk::ShotClinoRole, [&]() { return cwReading(QString()); });
            case ShotBackClinoRole:
                return data(cwSurveyChunk::ShotBackClinoRole, [&]() { return cwReading(QString()); });
            default:
                return QVariant();
            }
        }
        switch(role) {
        case ShotDistanceRole:
            return data(cwSurveyChunk::ShotDistanceRole, [&]() { return chunk->shot(shotIndex).distance(); });
        case ShotDistanceIncludedRole:
            return chunk->shot(shotIndex).isDistanceIncluded();
        case ShotCompassRole:
            return data(cwSurveyChunk::ShotCompassRole, [&]() { return chunk->shot(shotIndex).compass(); });
        case ShotBackCompassRole:
            return data(cwSurveyChunk::ShotBackCompassRole, [&]() { return chunk->shot(shotIndex).backCompass(); });
        case ShotClinoRole:
            return data(cwSurveyChunk::ShotClinoRole, [&]() { return chunk->shot(shotIndex).clino(); });
        case ShotBackClinoRole:
            return data(cwSurveyChunk::ShotBackClinoRole, [&]() { return chunk->shot(shotIndex).backClino(); });
        case ShotCalibrationRole:
            return QVariant::fromValue(static_cast<cwTripCalibration*>(nullptr));
        default:
            return QVariant();
        }
    };

    auto splayData = [role](const cwSurveyEditorRowIndex& chunkIndex)->QVariant {
        const auto chunk = chunkIndex.chunk();
        const int stationIndex = chunkIndex.indexInChunk();
        const int splayIndex = chunkIndex.splayIndex();

        if(role == SplayStationNameRole) {
            return chunk->station(stationIndex).name();
        }

        if(splayIndex < 0 || splayIndex >= chunk->stationSplayCount(stationIndex)) {
            return QVariant();
        }

        const cwShotMeasurement& splay = chunk->stationSplayAt(stationIndex, splayIndex);
        switch(role) {
        case SplayDistanceRole:
            return splay.distance.value();
        case SplayCompassRole:
            return splay.compass.value();
        case SplayClinoRole:
            return splay.clino.value();
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
    case cwSurveyEditorRowIndex::SplayRow:
        return splayData(rowIndex);
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
        count += chunkRowCount(chunk);
    }
    return count;
}

bool cwSurveyEditorModel::setDataAt(const cwSurveyEditorCellIndex& cell, const QVariant& data)
{
    if(m_trip.isNull()) {
        return false;
    }

    if(!isReadingCell(cell.dataRole())) {
        return false;
    }

    const int modelRow = cell.modelRow();
    if(modelRow < 0 || modelRow >= rowCount()) {
        return false;
    }

    const auto rowIndex = toRowIndex(modelRow);
    cwSurveyChunk* chunk = rowIndex.chunk();
    if(chunk == nullptr
            || rowIndex.rowType() == cwSurveyEditorRowIndex::TitleRow
            || rowIndex.rowType() != toRowType(cell.dataRole())
            || !m_trip->chunks().contains(chunk))
    {
        return false;
    }

    const QString stringData = data.toString();
    const bool isEmptyData = stringData.trimmed().isEmpty();

    const int indexInChunk = rowIndex.indexInChunk();
    const bool isStationVirtual = rowIndex.rowType() == cwSurveyEditorRowIndex::StationRow
                                  && indexInChunk == chunk->stationCount()
                                  && hasVirtualTrailingStationShot(chunk);
    const bool isShotVirtual = rowIndex.rowType() == cwSurveyEditorRowIndex::ShotRow
                               && indexInChunk == chunk->shotCount()
                               && hasVirtualTrailingStationShot(chunk);

    if(isStationVirtual || isShotVirtual) {
        if(isEmptyData) {
            return true;
        }

        const int oldRowCount = rowCount();
        disconnectChunkSignals(chunk);

        chunk->appendNewShot();
        chunk->setData(cell.dataRole(), indexInChunk, data);
        connectChunkSignals(chunk);

        const auto changedModelIndex = toModelIndex({chunk, indexInChunk, rowIndex.rowType()});
        if(changedModelIndex.isValid()) {
            emit dataChanged(changedModelIndex, changedModelIndex, changedRolesFor(cell.dataRole()));
        }

        beginInsertRows(QModelIndex(), oldRowCount, oldRowCount + 1);
        endInsertRows();
        return true;
    }

    if(indexInChunk < 0) {
        return false;
    }

    if(rowIndex.rowType() == cwSurveyEditorRowIndex::StationRow && indexInChunk >= chunk->stationCount()) {
        return false;
    }
    if(rowIndex.rowType() == cwSurveyEditorRowIndex::ShotRow && indexInChunk >= chunk->shotCount()) {
        return false;
    }

    chunk->setData(cell.dataRole(), indexInChunk, data);

    if(chunk == m_focusedChunk) {
        trim(chunk, FullTrim);
    }
    syncVirtualRows(chunk);
    return true;
}

QString cwSurveyEditorModel::guessStationNameAt(const cwSurveyEditorCellIndex& cell) const
{
    if(m_trip.isNull()) {
        return QString();
    }

    const int modelRow = cell.modelRow();
    if(modelRow < 0 || modelRow >= rowCount()) {
        return QString();
    }

    if(cell.dataRole() != cwSurveyChunk::StationNameRole) {
        return QString();
    }

    const auto rowIndex = toRowIndex(modelRow);
    cwSurveyChunk* chunk = rowIndex.chunk();
    if(chunk == nullptr
            || rowIndex.rowType() != cwSurveyEditorRowIndex::StationRow
            || !m_trip->chunks().contains(chunk))
    {
        return QString();
    }

    const int stationIndex = rowIndex.indexInChunk();
    if(stationIndex < 0) {
        return QString();
    }

    const bool hasVirtual = hasVisibleVirtualRows(chunk);
    const bool isVirtualStation = stationIndex == chunk->stationCount() && hasVirtual;
    const bool isLastRealStation = stationIndex == chunk->stationCount() - 1;
    if(!isVirtualStation && !isLastRealStation) {
        return QString();
    }

    if(!isVirtualStation) {
        const QString currentName = chunk->data(cwSurveyChunk::StationNameRole, stationIndex).toString();
        if(!currentName.isEmpty()) {
            return QString();
        }
    }

    QString guessedStationName = chunk->guessLastStationName();
    if(!guessedStationName.isEmpty()) {
        return guessedStationName;
    }

    if(isVirtualStation && stationIndex > 0) {
        const QString previousStationName = chunk->data(cwSurveyChunk::StationNameRole, stationIndex - 1).toString();
        guessedStationName = chunk->guessNextStation(previousStationName);
    }

    return guessedStationName;
}

bool cwSurveyEditorModel::shotDistanceIncludedAt(const cwSurveyEditorCellIndex& cell) const
{
    if(m_trip.isNull()) {
        return false;
    }

    const int modelRow = cell.modelRow();
    if(modelRow < 0 || modelRow >= rowCount()) {
        return false;
    }

    const auto rowIndex = toRowIndex(modelRow);
    cwSurveyChunk* chunk = rowIndex.chunk();
    if(chunk == nullptr
            || rowIndex.rowType() != cwSurveyEditorRowIndex::ShotRow
            || rowIndex.rowType() != toRowType(cell.dataRole())
            || !m_trip->chunks().contains(chunk))
    {
        return false;
    }

    const int indexInChunk = rowIndex.indexInChunk();
    if(indexInChunk < 0) {
        return false;
    }

    if(indexInChunk == chunk->shotCount()) {
        return hasVirtualTrailingStationShot(chunk);
    }

    if(indexInChunk > chunk->shotCount() - 1) {
        return false;
    }

    return chunk->data(cwSurveyChunk::ShotDistanceIncludedRole, indexInChunk).toBool();
}

bool cwSurveyEditorModel::canRemoveStationAt(const cwSurveyEditorCellIndex& cell,
                                             cwSurveyChunk::Direction direction) const
{
    if(m_trip.isNull()) {
        return false;
    }

    const int modelRow = cell.modelRow();
    if(modelRow < 0 || modelRow >= rowCount()) {
        return false;
    }

    const auto rowIndex = toRowIndex(modelRow);
    cwSurveyChunk* chunk = rowIndex.chunk();
    if(chunk == nullptr
            || rowIndex.rowType() != cwSurveyEditorRowIndex::StationRow
            || rowIndex.rowType() != toRowType(cell.dataRole())
            || !m_trip->chunks().contains(chunk))
    {
        return false;
    }

    const int indexInChunk = rowIndex.indexInChunk();
    if(indexInChunk < 0 || indexInChunk >= chunk->stationCount()) {
        return false;
    }

    return chunk->canRemoveStation(indexInChunk, direction);
}

bool cwSurveyEditorModel::canRemoveShotAt(const cwSurveyEditorCellIndex& cell,
                                          cwSurveyChunk::Direction direction) const
{
    if(m_trip.isNull()) {
        return false;
    }

    const int modelRow = cell.modelRow();
    if(modelRow < 0 || modelRow >= rowCount()) {
        return false;
    }

    const auto rowIndex = toRowIndex(modelRow);
    cwSurveyChunk* chunk = rowIndex.chunk();
    if(chunk == nullptr
            || rowIndex.rowType() != cwSurveyEditorRowIndex::ShotRow
            || rowIndex.rowType() != toRowType(cell.dataRole())
            || !m_trip->chunks().contains(chunk))
    {
        return false;
    }

    const int indexInChunk = rowIndex.indexInChunk();
    if(indexInChunk < 0 || indexInChunk >= chunk->shotCount()) {
        return false;
    }

    return chunk->canRemoveShot(indexInChunk, direction);
}

bool cwSurveyEditorModel::canInsertStationAt(const cwSurveyEditorCellIndex& cell,
                                             cwSurveyChunk::Direction direction) const
{
    if(m_trip.isNull()) {
        return false;
    }

    const int modelRow = cell.modelRow();
    if(modelRow < 0 || modelRow >= rowCount()) {
        return false;
    }

    const auto rowIndex = toRowIndex(modelRow);
    cwSurveyChunk* chunk = rowIndex.chunk();
    if(chunk == nullptr
            || rowIndex.rowType() != cwSurveyEditorRowIndex::StationRow
            || rowIndex.rowType() != toRowType(cell.dataRole())
            || !m_trip->chunks().contains(chunk))
    {
        return false;
    }

    const int indexInChunk = rowIndex.indexInChunk();
    const bool hasVirtual = hasVirtualTrailingStationShot(chunk);
    const bool isVirtualStation = indexInChunk == chunk->stationCount() && hasVirtual;
    if(isVirtualStation) {
        return false;
    }

    if(indexInChunk < 0 || indexInChunk >= chunk->stationCount()) {
        return false;
    }

    if(direction == cwSurveyChunk::Below
            && hasVirtual
            && indexInChunk == chunk->stationCount() - 1)
    {
        return false;
    }

    return true;
}

bool cwSurveyEditorModel::canInsertShotAt(const cwSurveyEditorCellIndex& cell,
                                          cwSurveyChunk::Direction direction) const
{
    if(m_trip.isNull()) {
        return false;
    }

    const int modelRow = cell.modelRow();
    if(modelRow < 0 || modelRow >= rowCount()) {
        return false;
    }

    const auto rowIndex = toRowIndex(modelRow);
    cwSurveyChunk* chunk = rowIndex.chunk();
    if(chunk == nullptr
            || rowIndex.rowType() != cwSurveyEditorRowIndex::ShotRow
            || rowIndex.rowType() != toRowType(cell.dataRole())
            || !m_trip->chunks().contains(chunk))
    {
        return false;
    }

    const int indexInChunk = rowIndex.indexInChunk();
    const bool hasVirtual = hasVirtualTrailingStationShot(chunk);
    const bool isVirtualShot = indexInChunk == chunk->shotCount() && hasVirtual;
    if(isVirtualShot) {
        return false;
    }

    if(indexInChunk < 0 || indexInChunk >= chunk->shotCount()) {
        return false;
    }

    if(direction == cwSurveyChunk::Below
            && hasVirtual
            && indexInChunk == chunk->shotCount() - 1)
    {
        return false;
    }

    return true;
}

void cwSurveyEditorModel::removeStationAt(const cwSurveyEditorCellIndex& cell,
                                          cwSurveyChunk::Direction direction)
{
    if(!canRemoveStationAt(cell, direction)) {
        return;
    }

    const auto rowIndex = toRowIndex(cell.modelRow());
    auto* chunk = rowIndex.chunk();
    if(chunk == nullptr) {
        return;
    }

    chunk->removeStation(rowIndex.indexInChunk(), direction);
}

void cwSurveyEditorModel::removeShotAt(const cwSurveyEditorCellIndex& cell,
                                       cwSurveyChunk::Direction direction)
{
    if(!canRemoveShotAt(cell, direction)) {
        return;
    }

    const auto rowIndex = toRowIndex(cell.modelRow());
    auto* chunk = rowIndex.chunk();
    if(chunk == nullptr) {
        return;
    }

    chunk->removeShot(rowIndex.indexInChunk(), direction);
}

void cwSurveyEditorModel::insertStationAt(const cwSurveyEditorCellIndex& cell,
                                          cwSurveyChunk::Direction direction)
{
    if(!canInsertStationAt(cell, direction)) {
        return;
    }

    const auto rowIndex = toRowIndex(cell.modelRow());
    auto* chunk = rowIndex.chunk();
    if(chunk == nullptr) {
        return;
    }

    const int indexInChunk = rowIndex.indexInChunk();
    const bool isVirtualStation = indexInChunk == chunk->stationCount()
                                  && hasVirtualTrailingStationShot(chunk);

    if(isVirtualStation) {
        chunk->appendNewShot();
        return;
    }

    if(indexInChunk < 0 || indexInChunk >= chunk->stationCount()) {
        return;
    }

    chunk->insertStation(indexInChunk, direction);
}

void cwSurveyEditorModel::insertShotAt(const cwSurveyEditorCellIndex& cell,
                                       cwSurveyChunk::Direction direction)
{
    if(!canInsertShotAt(cell, direction)) {
        return;
    }

    const auto rowIndex = toRowIndex(cell.modelRow());
    auto* chunk = rowIndex.chunk();
    if(chunk == nullptr) {
        return;
    }

    const int indexInChunk = rowIndex.indexInChunk();
    const bool isVirtualShot = indexInChunk == chunk->shotCount()
                               && hasVirtualTrailingStationShot(chunk);

    if(isVirtualShot) {
        chunk->appendNewShot();
        return;
    }

    if(indexInChunk < 0 || indexInChunk >= chunk->shotCount()) {
        return;
    }

    chunk->insertShot(indexInChunk, direction);
}

/**
 * @brief cwSurveyEditorModel::roleNames
 * @return
 */
QHash<int, QByteArray> cwSurveyEditorModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(RowIndexRole, "rowIndex");
    roles.insert(RowTypeRole, "rowType");
    roles.insert(IndexInChunkRole, "indexInChunk");
    roles.insert(ChunkRole, "chunk");
    roles.insert(IsVirtualRole, "isVirtual");
    roles.insert(StationNameRole, "stationName");
    roles.insert(StationLeftRole, "stationLeft");
    roles.insert(StationRightRole, "stationRight");
    roles.insert(StationUpRole, "stationUp");
    roles.insert(StationDownRole, "stationDown");
    roles.insert(StationFixedRole, "stationFixed");
    roles.insert(ShotDistanceRole, "shotDistance");
    roles.insert(ShotDistanceIncludedRole, "shotDistanceIncluded");
    roles.insert(ShotCompassRole, "shotCompass");
    roles.insert(ShotBackCompassRole, "shotBackCompass");
    roles.insert(ShotClinoRole, "shotClino");
    roles.insert(ShotBackClinoRole, "shotBackClino");
    roles.insert(ShotCalibrationRole, "shotCalibration");
    roles.insert(StationSplayCountRole, "stationSplayCount");
    roles.insert(StationSplaysExpandedRole, "stationSplaysExpanded");
    roles.insert(SplayDistanceRole, "splayDistance");
    roles.insert(SplayCompassRole, "splayCompass");
    roles.insert(SplayClinoRole, "splayClino");
    roles.insert(SplayStationNameRole, "splayStationName");
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

cwSurveyChunk* cwSurveyEditorModel::chunkForRow(int modelRow) const
{
    if(m_trip.isNull() || modelRow < 0 || modelRow >= rowCount()) {
        return nullptr;
    }
    return toRowIndex(modelRow).chunk();
}

bool cwSurveyEditorModel::isCellValid(const cwSurveyEditorCellIndex& cell) const
{
    if(m_trip.isNull()) {
        return false;
    }

    const int modelRow = cell.modelRow();
    if(modelRow < 0 || modelRow >= rowCount()) {
        return false;
    }

    const auto rowIndex = toRowIndex(modelRow);
    if(rowIndex.chunk() == nullptr || rowIndex.rowType() == cwSurveyEditorRowIndex::TitleRow) {
        return false;
    }

    return rowIndex.rowType() == toRowType(cell.dataRole());
}

int cwSurveyEditorModel::modelRowForChunkRole(cwSurveyChunk* chunk, int indexInChunk, cwSurveyChunk::DataRole role) const
{
    if(chunk == nullptr || m_trip.isNull() || !m_trip->chunks().contains(chunk)) {
        return -1;
    }
    return toModelRow(cwSurveyEditorRowIndex(chunk, indexInChunk, toRowType(role)));
}

bool cwSurveyEditorModel::isStationRole(cwSurveyChunk::DataRole role) const
{
    return toRowType(role) == cwSurveyEditorRowIndex::StationRow;
}

bool cwSurveyEditorModel::isShotRole(cwSurveyChunk::DataRole role) const
{
    return toRowType(role) == cwSurveyEditorRowIndex::ShotRow;
}

bool cwSurveyEditorModel::isCellSelected(const cwSurveyEditorCellIndex& selectedCell,
                                         const cwSurveyEditorCellIndex& candidateCell) const
{
    if(!isCellValid(selectedCell) || !isCellValid(candidateCell)) {
        return false;
    }

    if(selectedCell.dataRole() != candidateCell.dataRole()) {
        return false;
    }

    const auto selectedRowIndex = toRowIndex(selectedCell.modelRow());
    const auto candidateRowIndex = toRowIndex(candidateCell.modelRow());
    return selectedRowIndex == candidateRowIndex;
}

bool cwSurveyEditorModel::isFocusedCell(const cwSurveyEditorCellIndex& cell) const
{
    if(cell.modelRow() < 0 || cell.dataRole() < 0) {
        return false;
    }
    return focusedRow() == cell.modelRow() && focusedRole() == static_cast<int>(cell.dataRole());
}

int cwSurveyEditorModel::focusedRow() const
{
    return m_focusedRowIndex.isValid() ? m_focusedRowIndex.row() : -1;
}

int cwSurveyEditorModel::focusedRole() const
{
    return static_cast<int>(m_focusedDataRole);
}

void cwSurveyEditorModel::setFocusedCell(const cwSurveyEditorCellIndex& cell)
{
    if(!isCellValid(cell)) {
        return;
    }

    const int row = cell.modelRow();
    m_focusedRowIndex = QPersistentModelIndex(index(row, 0));
    m_focusedDataRole = cell.dataRole();
    setFocusedChunk(chunkForRow(row));
    syncFocusedCellSignals();
}

void cwSurveyEditorModel::dumpModel()
{
    const QList<cwSurveyChunk*> chunks = m_trip ? m_trip->chunks() : QList<cwSurveyChunk*>();
    qInfo() << "[SurveyEditorModelDump] begin rowCount=" << rowCount()
            << "focusedRow=" << focusedRow()
            << "focusedRole=" << focusedRole();

    for(int row = 0; row < rowCount(); ++row) {
        const cwSurveyEditorRowIndex rowIndex = toRowIndex(row);
        cwSurveyChunk* chunk = rowIndex.chunk();
        const int chunkIndex = chunks.indexOf(chunk);
        const auto rowType = rowIndex.rowType();
        const int indexInChunk = rowIndex.indexInChunk();
        QString stationName;
        QString shotDistance;

        if(chunk != nullptr) {
            if(rowType == cwSurveyEditorRowIndex::StationRow && indexInChunk >= 0) {
                stationName = chunk->data(cwSurveyChunk::StationNameRole, indexInChunk).toString();
            } else if(rowType == cwSurveyEditorRowIndex::ShotRow && indexInChunk >= 0) {
                const QVariant distanceValue = chunk->data(cwSurveyChunk::ShotDistanceRole, indexInChunk);
                if(distanceValue.canConvert<cwReading>()) {
                    shotDistance = distanceValue.value<cwReading>().value();
                } else {
                    shotDistance = distanceValue.toString();
                }
            }
        }

        qInfo() << "[SurveyEditorModelDump] row=" << row
                << "chunkIndex=" << chunkIndex
                << "rowType=" << rowType
                << "indexInChunk=" << indexInChunk
                << "isFocused=" << (focusedRow() == row)
                << "stationName=" << stationName
                << "shotDistance=" << shotDistance;
    }

    qInfo() << "[SurveyEditorModelDump] end";
}

void cwSurveyEditorModel::focusOnLastChunk()
{
    if(m_trip.isNull() || m_trip->chunkCount() <= 0) {
        return;
    }

    auto* lastChunk = m_trip->chunk(m_trip->chunkCount() - 1);
    if(lastChunk == nullptr || lastChunk->stationCount() <= 0) {
        return;
    }

    const int row = modelRowForChunkRole(lastChunk, 0, cwSurveyChunk::StationNameRole);
    setFocusedCell(cellIndex(row, cwSurveyChunk::StationNameRole));
}

cwSurveyEditorCellIndex cwSurveyEditorModel::nextCellIndex(const cwSurveyEditorCellIndex& currentCell,
                                                           NavigationKey key,
                                                           bool frontSights,
                                                           bool backSights) const
{
    if(!isCellValid(currentCell)) {
        return cwSurveyEditorCellIndex();
    }

    const auto currentRowIndex = toRowIndex(currentCell.modelRow());
    cwSurveyChunk* chunk = currentRowIndex.chunk();
    const int indexInChunk = currentRowIndex.indexInChunk();

    auto makeCell = [&](cwSurveyChunk* targetChunk,
                        int targetIndexInChunk,
                        cwSurveyEditorRowIndex::RowType targetRowType,
                        cwSurveyChunk::DataRole targetRole) {
        if(targetChunk == nullptr || m_trip.isNull()) {
            return cwSurveyEditorCellIndex();
        }
        const int row = toModelRow({targetChunk, targetIndexInChunk, targetRowType});
        if(row < 0) {
            return cwSurveyEditorCellIndex();
        }
        return cellIndex(row, targetRole);
    };

    auto offsetCell = [&](const cwSurveyEditorCellIndex& sourceCell, int offsetIndex) {
        if(offsetIndex == 0) {
            return sourceCell;
        }

        if(!isCellValid(sourceCell) || m_trip.isNull()) {
            return cwSurveyEditorCellIndex();
        }

        const auto sourceRowIndex = toRowIndex(sourceCell.modelRow());
        const auto rowType = sourceRowIndex.rowType();
        const auto chunks = m_trip->chunks();
        int currentChunkIndex = chunks.indexOf(sourceRowIndex.chunk());
        if(currentChunkIndex < 0) {
            return cwSurveyEditorCellIndex();
        }

        auto rowTypeCount = [this, rowType](const cwSurveyChunk* targetChunk) {
            if(rowType == cwSurveyEditorRowIndex::StationRow) {
                return stationCount(targetChunk);
            }
            if(rowType == cwSurveyEditorRowIndex::ShotRow) {
                return shotCount(targetChunk);
            }
            return -1;
        };

        int nextIndexInChunk = sourceRowIndex.indexInChunk() + offsetIndex;
        while(currentChunkIndex >= 0 && currentChunkIndex < chunks.size()) {
            const auto* currentChunk = chunks.at(currentChunkIndex);
            const int currentSize = rowTypeCount(currentChunk);

            if(currentSize <= 0) {
                if(nextIndexInChunk >= 0) {
                    ++currentChunkIndex;
                } else {
                    --currentChunkIndex;
                }
                continue;
            }

            if(nextIndexInChunk >= 0 && nextIndexInChunk < currentSize) {
                return makeCell(chunks.at(currentChunkIndex),
                                nextIndexInChunk,
                                rowType,
                                sourceCell.dataRole());
            }

            if(nextIndexInChunk < 0) {
                --currentChunkIndex;
                if(currentChunkIndex < 0) {
                    return cwSurveyEditorCellIndex();
                }
                nextIndexInChunk += rowTypeCount(chunks.at(currentChunkIndex));
            } else {
                nextIndexInChunk -= currentSize;
                ++currentChunkIndex;
            }
        }

        return cwSurveyEditorCellIndex();
    };

    auto makeCurrentRowRoleCell = [&](cwSurveyChunk::DataRole targetRole) {
        return makeCell(chunk, indexInChunk, currentRowIndex.rowType(), targetRole);
    };

    auto offsetCurrentRowRole = [&](cwSurveyChunk::DataRole targetRole, int offset) {
        return offsetCell(makeCurrentRowRoleCell(targetRole), offset);
    };

    auto stationCell = [&](int stationIndex, cwSurveyChunk::DataRole stationRole, int offset = 0) {
        return offsetCell(makeCell(chunk,
                                   stationIndex,
                                   cwSurveyEditorRowIndex::StationRow,
                                   stationRole),
                          offset);
    };

    auto shotCell = [&](int shotIndex, cwSurveyChunk::DataRole shotRole, int offset = 0) {
        return offsetCell(makeCell(chunk,
                                   shotIndex,
                                   cwSurveyEditorRowIndex::ShotRow,
                                   shotRole),
                          offset);
    };

    auto nextLeftFromClino = [&]() {
        const int stationIndex = indexInChunk == 0 ? indexInChunk : indexInChunk + 1;
        return stationCell(stationIndex, cwSurveyChunk::StationLeftRole, 0);
    };

    switch(currentCell.dataRole()) {
    case cwSurveyChunk::StationNameRole:
        switch(key) {
        case Tab:
            if(indexInChunk == 0) {
                return offsetCurrentRowRole(cwSurveyChunk::StationNameRole, 1);
            }
            return shotCell(indexInChunk - 1, cwSurveyChunk::ShotDistanceRole, 0);
        case BackTab:
        case Left:
            if(key == Left) {
                return cwSurveyEditorCellIndex();
            }
            if(indexInChunk == 1) {
                return offsetCurrentRowRole(cwSurveyChunk::StationNameRole, -1);
            }
            return offsetCurrentRowRole(cwSurveyChunk::StationSplaysRole, -1);
        case Right: {
            const int shotOffset = indexInChunk == 0 ? 0 : -1;
            return shotCell(indexInChunk + shotOffset, cwSurveyChunk::ShotDistanceRole, 0);
        }
        case Down:
            return offsetCurrentRowRole(cwSurveyChunk::StationNameRole, 1);
        case Up:
            return offsetCurrentRowRole(cwSurveyChunk::StationNameRole, -1);
        }
        break;
    case cwSurveyChunk::StationLeftRole:
        switch(key) {
        case Tab:
        case Right:
            return offsetCurrentRowRole(cwSurveyChunk::StationRightRole, 0);
        case BackTab:
        case Left:
            if(indexInChunk == 1) {
                return offsetCurrentRowRole(cwSurveyChunk::StationSplaysRole, -1);
            } else {
                const int shotOffset = indexInChunk == 0 ? 0 : -1;
                if(backSights) {
                    return shotCell(indexInChunk + shotOffset, cwSurveyChunk::ShotBackClinoRole, 0);
                } else if(frontSights) {
                    return shotCell(indexInChunk + shotOffset, cwSurveyChunk::ShotClinoRole, 0);
                }
                return shotCell(indexInChunk + shotOffset, cwSurveyChunk::ShotDistanceRole, 0);
            }
        case Down:
            return offsetCurrentRowRole(cwSurveyChunk::StationLeftRole, 1);
        case Up:
            return offsetCurrentRowRole(cwSurveyChunk::StationLeftRole, -1);
        }
        break;
    case cwSurveyChunk::StationRightRole:
        switch(key) {
        case Tab:
        case Right:
            return offsetCurrentRowRole(cwSurveyChunk::StationUpRole, 0);
        case BackTab:
        case Left:
            return offsetCurrentRowRole(cwSurveyChunk::StationLeftRole, 0);
        case Down:
            return offsetCurrentRowRole(cwSurveyChunk::StationRightRole, 1);
        case Up:
            return offsetCurrentRowRole(cwSurveyChunk::StationRightRole, -1);
        }
        break;
    case cwSurveyChunk::StationUpRole:
        switch(key) {
        case Tab:
        case Right:
            return offsetCurrentRowRole(cwSurveyChunk::StationDownRole, 0);
        case BackTab:
        case Left:
            return offsetCurrentRowRole(cwSurveyChunk::StationRightRole, 0);
        case Down:
            return offsetCurrentRowRole(cwSurveyChunk::StationUpRole, 1);
        case Up:
            return offsetCurrentRowRole(cwSurveyChunk::StationUpRole, -1);
        }
        break;
    case cwSurveyChunk::StationDownRole:
        switch(key) {
        case Tab:
        case Right:
            return offsetCurrentRowRole(cwSurveyChunk::StationSplaysRole, 0);
        case BackTab:
        case Left:
            return offsetCurrentRowRole(cwSurveyChunk::StationUpRole, 0);
        case Down:
            return offsetCurrentRowRole(cwSurveyChunk::StationDownRole, 1);
        case Up:
            return offsetCurrentRowRole(cwSurveyChunk::StationDownRole, -1);
        }
        break;
    case cwSurveyChunk::StationSplaysRole:
        //The last cell in a station row, so it inherits the way D used to leave
        //the row
        switch(key) {
        case Tab:
            if(indexInChunk == 0) {
                return offsetCurrentRowRole(cwSurveyChunk::StationLeftRole, 1);
            }
            return offsetCurrentRowRole(cwSurveyChunk::StationNameRole, 1);
        case BackTab:
        case Left:
            return offsetCurrentRowRole(cwSurveyChunk::StationDownRole, 0);
        case Right:
            return cwSurveyEditorCellIndex();
        case Down:
            return offsetCurrentRowRole(cwSurveyChunk::StationSplaysRole, 1);
        case Up:
            return offsetCurrentRowRole(cwSurveyChunk::StationSplaysRole, -1);
        }
        break;
    case cwSurveyChunk::ShotDistanceRole:
        switch(key) {
        case Tab:
            if(frontSights) {
                return offsetCurrentRowRole(cwSurveyChunk::ShotCompassRole, 0);
            } else if(backSights) {
                return offsetCurrentRowRole(cwSurveyChunk::ShotBackCompassRole, 0);
            }
            return stationCell(indexInChunk, cwSurveyChunk::StationLeftRole, 0);
        case Right:
            if(backSights) {
                return offsetCurrentRowRole(cwSurveyChunk::ShotBackCompassRole, 0);
            } else if(frontSights) {
                return offsetCurrentRowRole(cwSurveyChunk::ShotCompassRole, 0);
            }
            return stationCell(indexInChunk + 1, cwSurveyChunk::StationLeftRole, 0);
        case BackTab:
        case Left:
            return stationCell(indexInChunk + 1, cwSurveyChunk::StationNameRole, 0);
        case Down:
            return offsetCurrentRowRole(cwSurveyChunk::ShotDistanceRole, 1);
        case Up:
            return offsetCurrentRowRole(cwSurveyChunk::ShotDistanceRole, -1);
        }
        break;
    case cwSurveyChunk::ShotCompassRole:
        switch(key) {
        case Tab:
            if(backSights) {
                return offsetCurrentRowRole(cwSurveyChunk::ShotBackCompassRole, 0);
            } else if(frontSights) {
                return offsetCurrentRowRole(cwSurveyChunk::ShotClinoRole, 0);
            }
            return stationCell(indexInChunk + 1, cwSurveyChunk::StationLeftRole, 0);
        case BackTab:
        case Left:
            return offsetCurrentRowRole(cwSurveyChunk::ShotDistanceRole, 0);
        case Right:
            return offsetCurrentRowRole(cwSurveyChunk::ShotClinoRole, 0);
        case Down:
            if(backSights) {
                return offsetCurrentRowRole(cwSurveyChunk::ShotBackCompassRole, 0);
            }
            return offsetCurrentRowRole(cwSurveyChunk::ShotCompassRole, 1);
        case Up:
            if(backSights) {
                return offsetCurrentRowRole(cwSurveyChunk::ShotBackCompassRole, -1);
            }
            return offsetCurrentRowRole(cwSurveyChunk::ShotCompassRole, -1);
        }
        break;
    case cwSurveyChunk::ShotBackCompassRole:
        switch(key) {
        case Tab:
            if(frontSights) {
                return offsetCurrentRowRole(cwSurveyChunk::ShotClinoRole, 0);
            } else if(backSights) {
                return offsetCurrentRowRole(cwSurveyChunk::ShotBackClinoRole, 0);
            }
            return stationCell(indexInChunk + 1, cwSurveyChunk::StationLeftRole, 0);
        case BackTab:
            if(frontSights) {
                return offsetCurrentRowRole(cwSurveyChunk::ShotCompassRole, 0);
            }
            return offsetCurrentRowRole(cwSurveyChunk::ShotDistanceRole, 0);
        case Right:
            return offsetCurrentRowRole(cwSurveyChunk::ShotBackClinoRole, 0);
        case Left:
            return offsetCurrentRowRole(cwSurveyChunk::ShotDistanceRole, 0);
        case Down:
            if(frontSights) {
                return offsetCurrentRowRole(cwSurveyChunk::ShotCompassRole, 1);
            }
            return offsetCurrentRowRole(cwSurveyChunk::ShotBackCompassRole, 1);
        case Up:
            if(frontSights) {
                return offsetCurrentRowRole(cwSurveyChunk::ShotCompassRole, 0);
            }
            return offsetCurrentRowRole(cwSurveyChunk::ShotBackCompassRole, -1);
        }
        break;
    case cwSurveyChunk::ShotClinoRole:
        switch(key) {
        case Tab:
            if(backSights) {
                return offsetCurrentRowRole(cwSurveyChunk::ShotBackClinoRole, 0);
            }
            return nextLeftFromClino();
        case BackTab:
            if(backSights) {
                return offsetCurrentRowRole(cwSurveyChunk::ShotBackCompassRole, 0);
            } else if(frontSights) {
                return offsetCurrentRowRole(cwSurveyChunk::ShotCompassRole, 0);
            }
            return offsetCurrentRowRole(cwSurveyChunk::ShotDistanceRole, 0);
        case Right:
            return stationCell(indexInChunk + 1, cwSurveyChunk::StationLeftRole, 0);
        case Left:
            return offsetCurrentRowRole(cwSurveyChunk::ShotCompassRole, 0);
        case Down:
            if(backSights) {
                return offsetCurrentRowRole(cwSurveyChunk::ShotBackClinoRole, 0);
            }
            return offsetCurrentRowRole(cwSurveyChunk::ShotClinoRole, 1);
        case Up:
            if(backSights) {
                return offsetCurrentRowRole(cwSurveyChunk::ShotBackClinoRole, -1);
            }
            return offsetCurrentRowRole(cwSurveyChunk::ShotClinoRole, -1);
        }
        break;
    case cwSurveyChunk::ShotBackClinoRole:
        switch(key) {
        case Tab:
            return nextLeftFromClino();
        case BackTab:
            if(frontSights) {
                return offsetCurrentRowRole(cwSurveyChunk::ShotClinoRole, 0);
            } else if(backSights) {
                return offsetCurrentRowRole(cwSurveyChunk::ShotBackCompassRole, 0);
            }
            return offsetCurrentRowRole(cwSurveyChunk::ShotDistanceRole, 0);
        case Right:
            return stationCell(indexInChunk + 1, cwSurveyChunk::StationLeftRole, 0);
        case Left:
            return offsetCurrentRowRole(cwSurveyChunk::ShotBackCompassRole, 0);
        case Down:
            if(frontSights) {
                return offsetCurrentRowRole(cwSurveyChunk::ShotClinoRole, 1);
            }
            return offsetCurrentRowRole(cwSurveyChunk::ShotBackClinoRole, 1);
        case Up:
            if(frontSights) {
                return offsetCurrentRowRole(cwSurveyChunk::ShotClinoRole, 0);
            }
            return offsetCurrentRowRole(cwSurveyChunk::ShotBackClinoRole, -1);
        }
        break;
    default:
        break;
    }

    return cwSurveyEditorCellIndex();
}

cwSurveyEditorCellIndex cwSurveyEditorModel::nextCell(const cwSurveyEditorCellIndex& currentCell,
                                                      NavigationKey key,
                                                      bool frontSights,
                                                      bool backSights) const
{
    return nextCellIndex(currentCell, key, frontSights, backSights);
}

int cwSurveyEditorModel::toModelRow(const cwSurveyEditorRowIndex &rowIndex) const
{
    if (m_trip.isNull() || rowIndex.chunk() == nullptr) {
        return -1;
    }

    const QList<cwSurveyChunk*> allChunks = m_trip->chunks();
    const int chunkPosition = allChunks.indexOf(rowIndex.chunk());
    if (chunkPosition < 0) {
        return -1;
    }

    const auto rowChunk = rowIndex.chunk();
    switch (rowIndex.rowType()) {
    case cwSurveyEditorRowIndex::TitleRow:
        break;
    case cwSurveyEditorRowIndex::StationRow:
        if (rowIndex.indexInChunk() < 0 || rowIndex.indexInChunk() >= stationCount(rowChunk)) {
            return -1;
        }
        break;
    case cwSurveyEditorRowIndex::ShotRow:
        if (rowIndex.indexInChunk() < 0 || rowIndex.indexInChunk() >= shotCount(rowChunk)) {
            return -1;
        }
        break;
    case cwSurveyEditorRowIndex::SplayRow:
        if (rowIndex.indexInChunk() < 0 || rowIndex.indexInChunk() >= stationCount(rowChunk)) {
            return -1;
        }
        if (rowIndex.splayIndex() < 0
            || rowIndex.splayIndex() >= splayRowCount(rowChunk, rowIndex.indexInChunk()))
        {
            return -1;
        }
        break;
    default:
        return -1;
    }

    // Compute base model row for this chunk.
    int baseRow = 0;
    for (int i = 0; i < chunkPosition; ++i) {
        auto* current = allChunks.at(i);
        baseRow += chunkRowCount(current);
    }

    //Stations and shots still alternate; an open splay cluster pushes everything
    //below its station down by the rows it added
    const int indexInChunk = rowIndex.indexInChunk();
    int modelRow = -1;
    switch(rowIndex.rowType()) {
    case cwSurveyEditorRowIndex::TitleRow:
        modelRow = baseRow;
        break;
    case cwSurveyEditorRowIndex::StationRow:
        modelRow = baseRow + indexInChunk * 2 + m_titleRowOffset
                   + splayRowsBefore(rowChunk, indexInChunk);
        break;
    case cwSurveyEditorRowIndex::ShotRow:
        //Shot i sits below station i, so it clears that station's own cluster too
        modelRow = baseRow + indexInChunk * 2 + 1 + m_titleRowOffset
                   + splayRowsBefore(rowChunk, indexInChunk + 1);
        break;
    case cwSurveyEditorRowIndex::SplayRow:
        modelRow = baseRow + indexInChunk * 2 + m_titleRowOffset
                   + splayRowsBefore(rowChunk, indexInChunk)
                   + 1 + rowIndex.splayIndex();
        break;
    default:
        modelRow = -1;
        break;
    }

    if (modelRow < 0 || modelRow >= rowCount()) {
        return -1;
    }
    return modelRow;
}

/**
 * @brief cwSurveyEditorModel::toggleSplaysExpanded
 * @param rowIndex - The station row whose splay cluster opens or closes
 *
 * The expansion state lives here rather than in the delegate because the view
 * recycles delegates as it scrolls, which would throw the state away.
 */
void cwSurveyEditorModel::toggleSplaysExpanded(const cwSurveyEditorRowIndex& rowIndex)
{
    cwSurveyChunk* chunk = rowIndex.chunk();
    const int stationIndex = rowIndex.indexInChunk();

    if(chunk == nullptr
        || m_trip.isNull()
        || !m_trip->chunks().contains(chunk)
        || rowIndex.rowType() != cwSurveyEditorRowIndex::StationRow
        || stationIndex < 0
        || stationIndex >= chunk->stationCount())
    {
        return;
    }

    if(splayRowCount(chunk, stationIndex) > 0) {
        collapseSplays(chunk, stationIndex);
        return;
    }

    const int splayCount = chunk->stationSplayCount(stationIndex);
    if(splayCount == 0) {
        return;
    }

    const int stationRow = toModelRow({chunk, stationIndex, cwSurveyEditorRowIndex::StationRow});
    if(stationRow < 0) {
        return;
    }

    beginInsertRows(QModelIndex(), stationRow + 1, stationRow + splayCount);
    m_expandedSplays[chunk].insert(stationIndex, splayCount);
    endInsertRows();

    emitSplayExpansionChanged(chunk, stationIndex);
}

/**
 * @brief cwSurveyEditorModel::emitSplayExpansionChanged
 *
 * Two rows answer StationSplaysExpandedRole for \a stationIndex, so both hear
 * about it together: the station row, which draws the chip, and the shot row
 * below the cluster. That shot's boxes normally reach up over the boundary into
 * the station row above them, which is where an open cluster goes, so the shot
 * row needs the same news to drop them below the cluster instead.
 */
void cwSurveyEditorModel::emitSplayExpansionChanged(cwSurveyChunk* chunk, int stationIndex)
{
    const int stationRow = toModelRow({chunk, stationIndex, cwSurveyEditorRowIndex::StationRow});
    if(stationRow >= 0) {
        emit dataChanged(index(stationRow), index(stationRow), {StationSplaysExpandedRole});
    }

    const int shotRow = toModelRow({chunk, stationIndex, cwSurveyEditorRowIndex::ShotRow});
    if(shotRow >= 0) {
        emit dataChanged(index(shotRow), index(shotRow), {StationSplaysExpandedRole});
    }
}

void cwSurveyEditorModel::collapseSplays(cwSurveyChunk* chunk, int stationIndex)
{
    const int shownRows = splayRowCount(chunk, stationIndex);
    if(shownRows == 0) {
        return;
    }

    const int stationRow = toModelRow({chunk, stationIndex, cwSurveyEditorRowIndex::StationRow});
    Q_ASSERT(stationRow >= 0);

    beginRemoveRows(QModelIndex(), stationRow + 1, stationRow + shownRows);
    auto chunkIter = m_expandedSplays.find(chunk);
    chunkIter->remove(stationIndex);
    if(chunkIter->isEmpty()) {
        m_expandedSplays.erase(chunkIter);
    }
    endRemoveRows();

    emitSplayExpansionChanged(chunk, stationIndex);
}

/**
 * @brief cwSurveyEditorModel::reconcileSplayRows
 *
 * The station's splays changed underneath an open cluster, so the rows it shows
 * are grown or shrunk to match. The count is compared against the one remembered
 * in m_expandedSplays because the chunk only reports the station that changed.
 */
void cwSurveyEditorModel::reconcileSplayRows(cwSurveyChunk* chunk, int stationIndex)
{
    const int stationRow = toModelRow({chunk, stationIndex, cwSurveyEditorRowIndex::StationRow});
    if(stationRow < 0) {
        return;
    }

    const int shownRows = splayRowCount(chunk, stationIndex);
    const int splayCount = chunk->stationSplayCount(stationIndex);

    //A closed cluster only moves the chip's count; an open one grows or shrinks
    //to match, and the station row itself stays put either way
    if(shownRows > 0) {
        if(splayCount == 0) {
            collapseSplays(chunk, stationIndex);
        } else if(splayCount > shownRows) {
            beginInsertRows(QModelIndex(),
                            stationRow + 1 + shownRows,
                            stationRow + splayCount);
            m_expandedSplays[chunk].insert(stationIndex, splayCount);
            endInsertRows();
        } else if(splayCount < shownRows) {
            beginRemoveRows(QModelIndex(),
                            stationRow + 1 + splayCount,
                            stationRow + shownRows);
            m_expandedSplays[chunk].insert(stationIndex, splayCount);
            endRemoveRows();
        }
    }

    emit dataChanged(index(stationRow), index(stationRow), {StationSplayCountRole});

    if(splayRowCount(chunk, stationIndex) > 0) {
        emit dataChanged(index(stationRow + 1), index(stationRow + splayCount),
                         {SplayDistanceRole, SplayCompassRole, SplayClinoRole});
    }
}

/**
 * @brief cwSurveyEditorModel::shiftExpandedSplays
 *
 * Stations were added to or removed from \a chunk, so the open clusters at and
 * below \a firstStationIndex now belong to different stations. Their keys move
 * by \a offset to follow the stations they were opened on.
 */
void cwSurveyEditorModel::shiftExpandedSplays(cwSurveyChunk* chunk, int firstStationIndex, int offset)
{
    auto chunkIter = m_expandedSplays.find(chunk);
    if(offset == 0 || chunkIter == m_expandedSplays.end()) {
        return;
    }

    ExpandedSplays shifted;
    for(auto iter = chunkIter->constBegin(); iter != chunkIter->constEnd(); ++iter) {
        const int stationIndex = iter.key() < firstStationIndex ? iter.key() : iter.key() + offset;
        shifted.insert(stationIndex, iter.value());
    }
    *chunkIter = shifted;
}

cwSurveyEditorRowIndex::RowType cwSurveyEditorModel::toRowType(cwSurveyChunk::DataRole chunkDataRole) const
{
    switch(chunkDataRole) {
    case cwSurveyChunk::StationNameRole:
    case cwSurveyChunk::StationLeftRole:
    case cwSurveyChunk::StationRightRole:
    case cwSurveyChunk::StationUpRole:
    case cwSurveyChunk::StationDownRole:
    case cwSurveyChunk::StationSplaysRole:
        return cwSurveyEditorRowIndex::StationRow;
    case cwSurveyChunk::ShotDistanceRole:
    case cwSurveyChunk::ShotDistanceIncludedRole:
    case cwSurveyChunk::ShotCompassRole:
    case cwSurveyChunk::ShotBackCompassRole:
    case cwSurveyChunk::ShotClinoRole:
    case cwSurveyChunk::ShotBackClinoRole:
        return cwSurveyEditorRowIndex::ShotRow;
    default:
        Q_ASSERT(false);
        return cwSurveyEditorRowIndex::TitleRow;
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
    const int row = toModelRow(rowIndex);
    if (row < 0) {
        return QModelIndex();
    }
    return index(row);
}

cwSurveyEditorModel::Role cwSurveyEditorModel::toModelRole(cwSurveyChunk::DataRole chunkRole) const
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
    case cwSurveyChunk::StationSplaysRole:
        return StationSplayCountRole;
    case cwSurveyChunk::ShotDistanceIncludedRole:
        return ShotDistanceIncludedRole;
    case cwSurveyChunk::ShotDistanceRole:
        return ShotDistanceRole;
    case cwSurveyChunk::ShotCompassRole:
        return ShotCompassRole;
    case cwSurveyChunk::ShotBackCompassRole:
        return ShotBackCompassRole;
    case cwSurveyChunk::ShotClinoRole:
        return ShotClinoRole;
    case cwSurveyChunk::ShotBackClinoRole:
        return ShotBackClinoRole;
    default:
        Q_ASSERT(false);
        return StationNameRole;
    }
}

/**
 * The model roles a chunk-level change to \a chunkRole invalidates. Renaming a
 * station also moves it into or out of the cave's fixes, which no chunk role
 * reports — the fixes aren't the chunk's data.
 */
QList<int> cwSurveyEditorModel::changedRolesFor(cwSurveyChunk::DataRole chunkRole) const
{
    if(chunkRole == cwSurveyChunk::StationNameRole) {
        return {StationNameRole, StationFixedRole};
    }
    return {toModelRole(chunkRole)};
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
        auto diff = index - chunkRowCount(chunk);
        if(diff < 0) {
            index = index - m_titleRowOffset;
            if(index < 0) {
                //Title row
                return {chunk, -1, cwSurveyEditorRowIndex::TitleRow};
            }

            //Walk the open clusters above index to find how far they pushed it
            //down, or to land inside one of them
            int splayOffset = 0;
            if(const auto* expanded = expandedSplays(chunk)) {
                for(auto iter = expanded->constBegin(); iter != expanded->constEnd(); ++iter) {
                    const int stationRow = iter.key() * 2 + splayOffset;
                    if(index <= stationRow) {
                        break;
                    }
                    if(index <= stationRow + iter.value()) {
                        return {chunk, iter.key(), index - stationRow - 1,
                                cwSurveyEditorRowIndex::SplayRow};
                    }
                    splayOffset += iter.value();
                }
            }

            index -= splayOffset;

            if(index % 2 == 0) {
                //Is a station
                int stationIndex = index / 2;
                Q_ASSERT(stationIndex >= 0);
                Q_ASSERT(stationIndex < stationCount(chunk));
                return {chunk, stationIndex, cwSurveyEditorRowIndex::StationRow};
            } else {
                //Is a shot
                int shotIndex = index / 2;
                Q_ASSERT(shotIndex >= 0);
                Q_ASSERT(shotIndex < shotCount(chunk));
                return {chunk, shotIndex, cwSurveyEditorRowIndex::ShotRow};
            }
        }
        index = diff;
    }

    //Nothing found
    return cwSurveyEditorRowIndex();
}

int cwSurveyEditorModel::stationCount(const cwSurveyChunk* chunk) const
{
    if(chunk == nullptr) {
        return 0;
    }
    return chunk->stationCount() + (hasVirtualTrailingStationShot(chunk) ? 1 : 0);
}

int cwSurveyEditorModel::shotCount(const cwSurveyChunk* chunk) const
{
    if(chunk == nullptr) {
        return 0;
    }
    return chunk->shotCount() + (hasVirtualTrailingStationShot(chunk) ? 1 : 0);
}

int cwSurveyEditorModel::chunkRowCount(const cwSurveyChunk* chunk) const
{
    return stationCount(chunk) + shotCount(chunk) + m_titleRowOffset + splayRowsInChunk(chunk);
}

const cwSurveyEditorModel::ExpandedSplays* cwSurveyEditorModel::expandedSplays(const cwSurveyChunk* chunk) const
{
    //Every row of every chunk asks this, and almost always the answer is that
    //nothing is open anywhere, so that case costs one branch instead of a hash probe
    if(m_expandedSplays.isEmpty()) {
        return nullptr;
    }

    const auto iter = m_expandedSplays.constFind(chunk);
    return iter == m_expandedSplays.constEnd() ? nullptr : &iter.value();
}

/**
 * The number of splay rows station \a stationIndex is currently showing, which
 * is zero unless its cluster is open.
 */
int cwSurveyEditorModel::splayRowCount(const cwSurveyChunk* chunk, int stationIndex) const
{
    const auto* expanded = expandedSplays(chunk);
    return expanded == nullptr ? 0 : expanded->value(stationIndex, 0);
}

int cwSurveyEditorModel::splayRowsBefore(const cwSurveyChunk* chunk, int stationIndex) const
{
    const auto* expanded = expandedSplays(chunk);
    if(expanded == nullptr) {
        return 0;
    }

    int count = 0;
    for(auto iter = expanded->constBegin();
         iter != expanded->constEnd() && iter.key() < stationIndex;
         ++iter)
    {
        count += iter.value();
    }
    return count;
}

int cwSurveyEditorModel::splayRowsInChunk(const cwSurveyChunk* chunk) const
{
    //Every station is before the end, so this sums the whole chunk
    return splayRowsBefore(chunk, std::numeric_limits<int>::max());
}

/**
 * The row the chunk's virtual trailing station would sit on, which is just past
 * the rows its real stations, shots and open splay clusters take up.
 */
int cwSurveyEditorModel::firstVirtualRow(cwSurveyChunk* chunk) const
{
    const int baseRow = toModelRow({chunk, -1, cwSurveyEditorRowIndex::TitleRow});
    return baseRow + chunk->stationCount() + chunk->shotCount() + m_titleRowOffset
           + splayRowsInChunk(chunk);
}

bool cwSurveyEditorModel::hasVirtualTrailingStationShot(const cwSurveyChunk* chunk) const
{
    if(chunk == nullptr || chunk != m_focusedChunk || chunk->stationCount() == 0) {
        return false;
    }

    if(chunk->isStationAndShotsEmpty()) {
        return false;
    }

    return !isStationShotEmpty(const_cast<cwSurveyChunk*>(chunk), chunk->stationCount() - 1);
}

bool cwSurveyEditorModel::hasVisibleVirtualRows(const cwSurveyChunk* chunk) const
{
    return chunk != nullptr && chunk == m_virtualRowsVisibleChunk;
}

void cwSurveyEditorModel::syncFocusedCellSignals()
{
    if(!m_focusedRowIndex.isValid()) {
        m_focusedDataRole = static_cast<cwSurveyChunk::DataRole>(-1);
    }

    const int row = focusedRow();
    const int role = focusedRole();

    if(m_lastNotifiedFocusedRow != row) {
        m_lastNotifiedFocusedRow = row;
        emit focusedRowChanged();
    }

    if(m_lastNotifiedFocusedRole != role) {
        m_lastNotifiedFocusedRole = role;
        emit focusedRoleChanged();
    }
}

void cwSurveyEditorModel::syncVirtualRows(cwSurveyChunk* chunk)
{
    if(chunk == nullptr
            || chunk != m_focusedChunk
            || m_trip.isNull()
            || !m_trip->chunks().contains(chunk))
    {
        return;
    }

    const bool shouldHaveVirtualRows = hasVirtualTrailingStationShot(chunk);
    const bool hasVirtualRows = hasVisibleVirtualRows(chunk);

    if(shouldHaveVirtualRows == hasVirtualRows) {
        return;
    }

    const int first = firstVirtualRow(chunk);
    const int last = first + 1;

    if(shouldHaveVirtualRows) {
        beginInsertRows(QModelIndex(), first, last);
        m_virtualRowsVisibleChunk = chunk;
        endInsertRows();
    } else {
        beginRemoveRows(QModelIndex(), first, last);
        if(m_virtualRowsVisibleChunk == chunk) {
            m_virtualRowsVisibleChunk = nullptr;
        }
        endRemoveRows();
    }
}

void cwSurveyEditorModel::trim(cwSurveyChunk* chunk)
{
    if(chunk == nullptr) {
        return;
    }

    if(chunk->stationCount() <= 2) {
        if(isStationShotEmpty(chunk, 1)) {
            chunk->removeStation(1, cwSurveyChunk::Above);
        }

        if(isStationShotEmpty(chunk, 0)) {
            chunk->removeStation(0, cwSurveyChunk::Above);
        }
    }

    trim(chunk, FullTrim);
}

void cwSurveyEditorModel::trim(cwSurveyChunk* chunk, TrimType trimType)
{
    if(chunk == nullptr || chunk->stationCount() <= 2) {
        return;
    }

    switch(trimType) {
    case FullTrim: {
        for(int i = chunk->stationCount() - 1; i > 1; i--) {
            if(isStationShotEmpty(chunk, i)) {
                chunk->removeStation(i, cwSurveyChunk::Above);
            }
        }
        break;
    }
    case PreserveLastEmptyOne: {
        for(int i = chunk->stationCount() - 1; i > 1; i--) {
            if(isStationShotEmpty(chunk, i - 1) && isStationShotEmpty(chunk, i)) {
                chunk->removeStation(i, cwSurveyChunk::Above);
            }
        }
        break;
    }
    }
}

bool cwSurveyEditorModel::isStationShotEmpty(cwSurveyChunk* chunk, int stationIndex)
{
    if(chunk == nullptr || stationIndex == 0 || stationIndex >= chunk->stationCount()) {
        return false;
    }

    cwStation station = chunk->station(stationIndex);
    cwShot shot = chunk->shot(stationIndex - 1);

    return station.name().isEmpty() &&
        station.left().state() == cwDistanceReading::State::Empty &&
        station.right().state() == cwDistanceReading::State::Empty &&
        station.down().state() == cwDistanceReading::State::Empty &&
        station.up().state() == cwDistanceReading::State::Empty &&
        shot.distance().state() == cwDistanceReading::State::Empty &&
        shot.compass().state() == cwCompassReading::State::Empty &&
        shot.clino().state() == cwClinoReading::State::Empty &&
        shot.backCompass().state() == cwCompassReading::State::Empty &&
        shot.backClino().state() == cwClinoReading::State::Empty;
}
