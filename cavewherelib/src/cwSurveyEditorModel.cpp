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
#include <algorithm>
#include <array>
#include <limits>

namespace {
    //! The blank row an open cluster carries under its splays, which is where
    //! the station's next splay is typed
    constexpr int kBlankSplayRowCount = 1;
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

    auto chunkCellChange = [this, chunk](cwSurveyChunk::DataRole role, int chunkIndex) {
        auto rowType = toRowType(cwSurveyEditorCellIndex::toCellRole(role));
        auto modelIndex = toModelIndex({chunk, chunkIndex, rowType});
        if(!modelIndex.isValid()) {
            syncVirtualRows(chunk);
            return;
        }
        emit dataChanged(modelIndex, modelIndex, changedRolesFor(role));
        syncVirtualRows(chunk);
    };

    //An open cluster labels each of its rows with the station's name, so a
    //rename has to reach the rows hanging below the station row too. Only a
    //rename does — an error appearing on the station name leaves the labels
    //reading exactly what they read before
    auto renameSplayLabels = [this, chunk](cwSurveyChunk::DataRole role, int chunkIndex) {
        if(role != cwSurveyChunk::StationNameRole) {
            return;
        }

        const int stationRow = toModelRow({chunk, chunkIndex, cwSurveyEditorRowIndex::StationRow});
        const int splayRows = splayRowCount(chunk, chunkIndex);
        if(stationRow >= 0 && splayRows > 0) {
            emit dataChanged(index(stationRow + 1),
                             index(stationRow + splayRows),
                             {SplayStationNameRole});
        }
    };

    connect(chunk, &cwSurveyChunk::dataChanged, this, chunkCellChange);
    connect(chunk, &cwSurveyChunk::dataChanged, this, renameSplayLabels);
    connect(chunk, &cwSurveyChunk::errorsChanged, this, chunkCellChange);

    connect(chunk, &cwSurveyChunk::stationSplaysChanged, this, [this, chunk](int stationIndex) {
        //An armed move names its splays by index, so a splay leaving or joining
        //the station they're coming off renumbers them out from under it
        if(m_pendingSplayMove.chunk == chunk && m_pendingSplayMove.stationIndex == stationIndex) {
            cancelSplayMove();
        }

        reconcileSplayRows(chunk, stationIndex);
    });

    connect(chunk, &cwSurveyChunk::added, this,
            [this, chunk, emitDataChangedForChunk](int stationBegin, int stationEnd, int shotBegin, int shotEnd) {
                //The stations at and below the insert renumber, so an armed
                //move would follow its old index onto somebody else's splays
                if(m_pendingSplayMove.chunk == chunk) {
                    cancelSplayMove();
                }

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
                //The stations left over renumber, so an armed move would follow
                //its old index onto somebody else's splays
                if(m_pendingSplayMove.chunk == chunk) {
                    cancelSplayMove();
                }

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
        //A move armed in the trip the table is leaving has nowhere to land
        cancelSplayMove();

        beginResetModel();
        m_focusedChunk = nullptr;
        m_virtualRowsVisibleChunk = nullptr;
        m_focusedRowIndex = QPersistentModelIndex();
        m_focusedCellRole = static_cast<cwSurveyEditorCellIndex::CellRole>(-1);
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
                            if(m_pendingSplayMove.chunk == chunk) {
                                cancelSplayMove();
                            }
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

        //The blank row at the bottom of an open cluster stands for the splay
        //the station hasn't been given yet, so it sits past the last real one
        if(rowIndex.rowType() == cwSurveyEditorRowIndex::SplayRow) {
            return rowIndex.splayIndex()
                   == rowIndex.chunk()->stationSplayCount(rowIndex.indexInChunk());
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

        //The row draws the rail tying the cluster together, so it needs to know
        //where in the cluster it sits, not just which splay it shows
        if(role == StationSplayCountRole) {
            return chunk->stationSplayCount(stationIndex);
        }

        const int splayCount = chunk->stationSplayCount(stationIndex);
        if(splayIndex < 0 || splayIndex > splayCount) {
            return QVariant();
        }

        //A splay reading is edited through the same boxes the rest of the table
        //uses, so it arrives in the same shape they read. Nothing checks a splay
        //yet, so it carries no error model
        auto splayReading = [&chunkIndex](const cwReading& reading,
                                          cwSurveyChunk::DataRole readingRole) {
            return QVariant::fromValue(cwSurveyEditorBoxData(reading, chunkIndex, readingRole));
        };

        //The blank row reads empty in all three columns until the first reading
        //typed into it makes it a splay
        const bool isBlankRow = splayIndex == splayCount;
        const cwShotMeasurement splay = isBlankRow
                                            ? cwShotMeasurement()
                                            : chunk->stationSplayAt(stationIndex, splayIndex);
        switch(role) {
        case SplayDistanceRole:
            return splayReading(splay.distance, cwSurveyChunk::ShotDistanceRole);
        case SplayCompassRole:
            return splayReading(splay.compass, cwSurveyChunk::ShotCompassRole);
        case SplayClinoRole:
            return splayReading(splay.clino, cwSurveyChunk::ShotClinoRole);
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

    //A splay's readings hang off a station instead of sitting in a row of the
    //chunk, so they're written through the cluster rather than chunk->setData
    const auto splayReadingRole = cwSurveyEditorCellIndex::toSplayReadingRole(cell.cellRole());
    if(splayReadingRole.has_value()) {
        return setSplayDataAt(cell, *splayReadingRole, data);
    }

    //A cell the editor owns rather than one the chunk stores has no reading to
    //write to
    const auto chunkRole = cwSurveyEditorCellIndex::toChunkRole(cell.cellRole());
    if(!chunkRole.has_value()) {
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
            || rowIndex.rowType() != toRowType(cell.cellRole())
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
        chunk->setData(*chunkRole, indexInChunk, data);
        connectChunkSignals(chunk);

        const auto changedModelIndex = toModelIndex({chunk, indexInChunk, rowIndex.rowType()});
        if(changedModelIndex.isValid()) {
            emit dataChanged(changedModelIndex, changedModelIndex, changedRolesFor(*chunkRole));
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

    chunk->setData(*chunkRole, indexInChunk, data);

    if(chunk == m_focusedChunk) {
        trim(chunk, FullTrim);
    }
    syncVirtualRows(chunk);
    return true;
}

/**
 * @brief cwSurveyEditorModel::setSplayDataAt
 * @param cell - One of the three reading cells of a splay row
 * @param readingRole - The reading \a cell shows, from toSplayReadingRole
 * @return True when the reading was handed to the chunk
 *
 * The row pins the chunk, the station and the splay, so the cell only has to
 * say which of the splay's three readings it holds. The blank row at the bottom
 * of the cluster is where a splay is made: the first reading written into it
 * appends one, and reconciliation turns the row into data and hands the cluster
 * a fresh blank underneath.
 */
bool cwSurveyEditorModel::setSplayDataAt(const cwSurveyEditorCellIndex& cell,
                                         cwSurveyChunk::DataRole readingRole,
                                         const QVariant& data)
{
    if(!isCellValid(cell)) {
        return false;
    }

    const auto rowIndex = toRowIndex(cell.modelRow());
    cwSurveyChunk* chunk = splayClusterChunk(rowIndex);
    if(chunk == nullptr) {
        return false;
    }

    const int stationIndex = rowIndex.indexInChunk();
    const int splayIndex = rowIndex.splayIndex();
    const int splayCount = chunk->stationSplayCount(stationIndex);
    if(splayIndex < 0 || splayIndex > splayCount) {
        return false;
    }

    if(splayIndex == splayCount) {
        //Tabbing across the blank row without typing leaves it blank, so the
        //station keeps exactly one of them
        if(data.toString().trimmed().isEmpty()) {
            return true;
        }

        chunk->appendStationSplay(stationIndex, cwShotMeasurement());
    }

    chunk->setStationSplayData(readingRole, stationIndex, splayIndex, data);
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

    if(cell.cellRole() != cwSurveyEditorCellIndex::StationNameCell) {
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
            || rowIndex.rowType() != toRowType(cell.cellRole())
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
            || rowIndex.rowType() != toRowType(cell.cellRole())
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
            || rowIndex.rowType() != toRowType(cell.cellRole())
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
            || rowIndex.rowType() != toRowType(cell.cellRole())
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
            || rowIndex.rowType() != toRowType(cell.cellRole())
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

    return rowIndex.rowType() == toRowType(cell.cellRole());
}

int cwSurveyEditorModel::modelRowForCellRole(cwSurveyChunk* chunk, int indexInChunk, cwSurveyEditorCellIndex::CellRole role) const
{
    if(chunk == nullptr || m_trip.isNull() || !m_trip->chunks().contains(chunk)) {
        return -1;
    }
    return toModelRow(cwSurveyEditorRowIndex(chunk, indexInChunk, toRowType(role)));
}

bool cwSurveyEditorModel::isStationCell(cwSurveyEditorCellIndex::CellRole role) const
{
    return toRowType(role) == cwSurveyEditorRowIndex::StationRow;
}

bool cwSurveyEditorModel::isShotCell(cwSurveyEditorCellIndex::CellRole role) const
{
    return toRowType(role) == cwSurveyEditorRowIndex::ShotRow;
}

bool cwSurveyEditorModel::isSplayCell(cwSurveyEditorCellIndex::CellRole role) const
{
    return toRowType(role) == cwSurveyEditorRowIndex::SplayRow;
}

bool cwSurveyEditorModel::isCellSelected(const cwSurveyEditorCellIndex& selectedCell,
                                         const cwSurveyEditorCellIndex& candidateCell) const
{
    if(!isCellValid(selectedCell) || !isCellValid(candidateCell)) {
        return false;
    }

    if(selectedCell.cellRole() != candidateCell.cellRole()) {
        return false;
    }

    const auto selectedRowIndex = toRowIndex(selectedCell.modelRow());
    const auto candidateRowIndex = toRowIndex(candidateCell.modelRow());
    return selectedRowIndex == candidateRowIndex;
}

bool cwSurveyEditorModel::isFocusedCell(const cwSurveyEditorCellIndex& cell) const
{
    if(cell.modelRow() < 0 || cell.cellRole() < 0) {
        return false;
    }
    return focusedRow() == cell.modelRow() && focusedRole() == static_cast<int>(cell.cellRole());
}

int cwSurveyEditorModel::focusedRow() const
{
    return m_focusedRowIndex.isValid() ? m_focusedRowIndex.row() : -1;
}

int cwSurveyEditorModel::focusedRole() const
{
    return static_cast<int>(m_focusedCellRole);
}

void cwSurveyEditorModel::setFocusedCell(const cwSurveyEditorCellIndex& cell)
{
    if(!isCellValid(cell)) {
        return;
    }

    const int row = cell.modelRow();
    m_focusedRowIndex = QPersistentModelIndex(index(row, 0));
    m_focusedCellRole = cell.cellRole();
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

    const int row = modelRowForCellRole(lastChunk, 0, cwSurveyEditorCellIndex::StationNameCell);
    setFocusedCell(cellIndex(row, cwSurveyEditorCellIndex::StationNameCell));
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
                        cwSurveyEditorCellIndex::CellRole targetRole) {
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
                                sourceCell.cellRole());
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

    auto makeCurrentRowRoleCell = [&](cwSurveyEditorCellIndex::CellRole targetRole) {
        return makeCell(chunk, indexInChunk, currentRowIndex.rowType(), targetRole);
    };

    auto offsetCurrentRowRole = [&](cwSurveyEditorCellIndex::CellRole targetRole, int offset) {
        return offsetCell(makeCurrentRowRoleCell(targetRole), offset);
    };

    auto stationCell = [&](int stationIndex, cwSurveyEditorCellIndex::CellRole stationRole, int offset = 0) {
        return offsetCell(makeCell(chunk,
                                   stationIndex,
                                   cwSurveyEditorRowIndex::StationRow,
                                   stationRole),
                          offset);
    };

    auto shotCell = [&](int shotIndex, cwSurveyEditorCellIndex::CellRole shotRole, int offset = 0) {
        return offsetCell(makeCell(chunk,
                                   shotIndex,
                                   cwSurveyEditorRowIndex::ShotRow,
                                   shotRole),
                          offset);
    };

    auto nextLeftFromClino = [&]() {
        const int stationIndex = indexInChunk == 0 ? indexInChunk : indexInChunk + 1;
        return stationCell(stationIndex, cwSurveyEditorCellIndex::StationLeftCell, 0);
    };

    //A splay is one front sight, so its readings chain straight across and the
    //front/back sight branching the shot rows do stays off this path
    constexpr std::array<cwSurveyEditorCellIndex::CellRole, 3> kSplayColumns = {
        cwSurveyEditorCellIndex::SplayDistanceCell,
        cwSurveyEditorCellIndex::SplayCompassCell,
        cwSurveyEditorCellIndex::SplayClinoCell
    };

    auto splayCell = [&](int splayIndex, cwSurveyEditorCellIndex::CellRole splayRole) {
        if(chunk == nullptr || m_trip.isNull()) {
            return cwSurveyEditorCellIndex();
        }
        const int row = toModelRow(cwSurveyEditorRowIndex(chunk,
                                                          indexInChunk,
                                                          splayIndex,
                                                          cwSurveyEditorRowIndex::SplayRow));
        if(row < 0) {
            return cwSurveyEditorCellIndex();
        }
        return cellIndex(row, splayRole);
    };

    //The cell that owns the cluster, which is the row directly above it
    auto clusterCell = [&]() {
        return makeCell(chunk,
                        indexInChunk,
                        cwSurveyEditorRowIndex::StationRow,
                        cwSurveyEditorCellIndex::StationSplaysCell);
    };

    //Where a splay column sits in kSplayColumns, so the column ordering lives
    //in the array alone
    auto splayColumn = [&](cwSurveyEditorCellIndex::CellRole splayRole) {
        const auto column = std::find(kSplayColumns.begin(), kSplayColumns.end(), splayRole);
        return static_cast<int>(std::distance(kSplayColumns.begin(), column));
    };

    //How a station row carries on forward, past the cluster it owns. The
    //cluster's last row leaves the way the cell that owns it does, so both read
    //it from here
    auto tabOutOfStationRow = [&]() {
        if(indexInChunk == 0) {
            return stationCell(indexInChunk, cwSurveyEditorCellIndex::StationLeftCell, 1);
        }
        return stationCell(indexInChunk, cwSurveyEditorCellIndex::StationNameCell, 1);
    };

    //Shift-tab back into a station row lands on the cluster the row has open
    //rather than the cell that owns it: the cluster's rows come after that cell
    //in the chain, so the last of them is what tab last left. \a splaysCell is
    //returned as it is for a station whose cluster is closed
    auto backTabIntoStationRow = [&](const cwSurveyEditorCellIndex& splaysCell) {
        if(!isCellValid(splaysCell)) {
            return splaysCell;
        }

        const auto stationRowIndex = toRowIndex(splaysCell.modelRow());
        const int stationIndex = stationRowIndex.indexInChunk();
        const int lastSplayRow = splayRowCount(stationRowIndex.chunk(), stationIndex) - 1;
        const int row = toModelRow(cwSurveyEditorRowIndex(stationRowIndex.chunk(),
                                                          stationIndex,
                                                          lastSplayRow,
                                                          cwSurveyEditorRowIndex::SplayRow));
        if(row < 0) {
            return splaysCell;
        }
        return cellIndex(row, kSplayColumns.back());
    };

    //The row above or below \a fromRow that the distance, compass and clino
    //columns run through. Only splay rows and shot rows carry them — a station
    //row's cells sit in columns of their own — so a column steps over it
    auto readingRowNear = [&](int fromRow, int direction) {
        const int rows = rowCount();
        for(int row = fromRow + direction; row >= 0 && row < rows; row += direction) {
            switch(toRowIndex(row).rowType()) {
            case cwSurveyEditorRowIndex::SplayRow:
            case cwSurveyEditorRowIndex::ShotRow:
                return row;
            case cwSurveyEditorRowIndex::TitleRow:
            case cwSurveyEditorRowIndex::StationRow:
                break;
            }
        }
        return -1;
    };

    //Where a column of splay readings lands on a shot row. A shot's compass and
    //clino are drawn only for the sights the trip records, so a row entered from
    //above lands on the sight it shows first and one entered from below on the
    //sight it shows last; with neither sight group recorded the whole column
    //lands on the distance cell
    auto shotCellForSplayColumn = [&](int shotRow, int column, bool fromAbove) {
        if(!frontSights && !backSights) {
            return cellIndex(shotRow, cwSurveyEditorCellIndex::ShotDistanceCell);
        }

        const bool backSight = fromAbove ? (backSights && !frontSights) : backSights;
        switch(kSplayColumns.at(column)) {
        case cwSurveyEditorCellIndex::SplayCompassCell:
            return cellIndex(shotRow,
                             backSight ? cwSurveyEditorCellIndex::ShotBackCompassCell
                                       : cwSurveyEditorCellIndex::ShotCompassCell);
        case cwSurveyEditorCellIndex::SplayClinoCell:
            return cellIndex(shotRow,
                             backSight ? cwSurveyEditorCellIndex::ShotBackClinoCell
                                       : cwSurveyEditorCellIndex::ShotClinoCell);
        default:
            return cellIndex(shotRow, cwSurveyEditorCellIndex::ShotDistanceCell);
        }
    };

    //One step up or down a splay's column: the cluster's next row, the cluster
    //the neighboring station has open, or the shot row the column runs onto.
    //\a atEnd answers for a column that runs off the end of the table
    auto splayColumnStep = [&](int column, int direction, const cwSurveyEditorCellIndex& atEnd) {
        const int row = readingRowNear(currentCell.modelRow(), direction);
        if(row < 0) {
            return atEnd;
        }
        if(toRowIndex(row).rowType() == cwSurveyEditorRowIndex::SplayRow) {
            return cellIndex(row, kSplayColumns.at(column));
        }
        return shotCellForSplayColumn(row, column, direction > 0);
    };

    auto nextSplayCell = [&](cwSurveyEditorCellIndex::CellRole splayRole) {
        const int column = splayColumn(splayRole);
        const int splayIndex = currentRowIndex.splayIndex();
        const int lastColumn = static_cast<int>(kSplayColumns.size()) - 1;

        switch(key) {
        case Tab: {
            if(column < lastColumn) {
                return splayCell(splayIndex, kSplayColumns.at(column + 1));
            }
            const auto nextSplay = splayCell(splayIndex + 1, cwSurveyEditorCellIndex::SplayDistanceCell);
            if(isCellValid(nextSplay)) {
                return nextSplay;
            }
            //Out the bottom of the cluster, the row carries on the way the cell
            //that owns the cluster leaves it
            return tabOutOfStationRow();
        }
        case BackTab:
            if(column > 0) {
                return splayCell(splayIndex, kSplayColumns.at(column - 1));
            }
            if(splayIndex > 0) {
                return splayCell(splayIndex - 1, kSplayColumns.back());
            }
            return clusterCell();
        case Left:
            if(column > 0) {
                return splayCell(splayIndex, kSplayColumns.at(column - 1));
            }
            return cwSurveyEditorCellIndex();
        case Right:
            if(column < lastColumn) {
                return splayCell(splayIndex, kSplayColumns.at(column + 1));
            }
            return cwSurveyEditorCellIndex();
        case Up:
            //A cluster at the head of the table has no reading above it, so the
            //caret falls back on the cell the cluster hangs from
            return splayColumnStep(column, -1, clusterCell());
        case Down:
            return splayColumnStep(column, 1, cwSurveyEditorCellIndex());
        }

        return cwSurveyEditorCellIndex();
    };

    //The splay column a shot's reading shares. A splay's three readings sit in
    //the shot's distance, compass and clino columns, so a cluster standing
    //between two shot rows is walked in the column the caret is already in
    auto splayColumnOfShotCell = [](cwSurveyEditorCellIndex::CellRole shotRole) {
        switch(shotRole) {
        case cwSurveyEditorCellIndex::ShotCompassCell:
        case cwSurveyEditorCellIndex::ShotBackCompassCell:
            return cwSurveyEditorCellIndex::SplayCompassCell;
        case cwSurveyEditorCellIndex::ShotClinoCell:
        case cwSurveyEditorCellIndex::ShotBackClinoCell:
            return cwSurveyEditorCellIndex::SplayClinoCell;
        default:
            return cwSurveyEditorCellIndex::SplayDistanceCell;
        }
    };

    //A shot row's Up and Down step into an open cluster standing next to it
    //before carrying on to the shot row past it. Going down that cluster is
    //entered at its first row and going up at its last, which is the row the
    //walk reaches first either way
    auto shotRowStep = [&](cwSurveyEditorCellIndex::CellRole leavingRole, int direction) {
        const int row = readingRowNear(currentCell.modelRow(), direction);
        if(row >= 0 && toRowIndex(row).rowType() == cwSurveyEditorRowIndex::SplayRow) {
            return cellIndex(row, splayColumnOfShotCell(currentCell.cellRole()));
        }
        return offsetCurrentRowRole(leavingRole, direction);
    };

    switch(currentCell.cellRole()) {
    case cwSurveyEditorCellIndex::StationNameCell:
        switch(key) {
        case Tab:
            if(indexInChunk == 0) {
                return offsetCurrentRowRole(cwSurveyEditorCellIndex::StationNameCell, 1);
            }
            return shotCell(indexInChunk - 1, cwSurveyEditorCellIndex::ShotDistanceCell, 0);
        case BackTab:
        case Left:
            if(key == Left) {
                return cwSurveyEditorCellIndex();
            }
            if(indexInChunk == 1) {
                return offsetCurrentRowRole(cwSurveyEditorCellIndex::StationNameCell, -1);
            }
            return backTabIntoStationRow(
                offsetCurrentRowRole(cwSurveyEditorCellIndex::StationSplaysCell, -1));
        case Right: {
            const int shotOffset = indexInChunk == 0 ? 0 : -1;
            return shotCell(indexInChunk + shotOffset, cwSurveyEditorCellIndex::ShotDistanceCell, 0);
        }
        case Down:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::StationNameCell, 1);
        case Up:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::StationNameCell, -1);
        }
        break;
    case cwSurveyEditorCellIndex::StationLeftCell:
        switch(key) {
        case Tab:
        case Right:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::StationRightCell, 0);
        case BackTab:
        case Left:
            if(indexInChunk == 1) {
                const auto splaysCell =
                    offsetCurrentRowRole(cwSurveyEditorCellIndex::StationSplaysCell, -1);
                //Only the tab chain walks the cluster; Left stays in its row
                return key == BackTab ? backTabIntoStationRow(splaysCell) : splaysCell;
            } else {
                const int shotOffset = indexInChunk == 0 ? 0 : -1;
                if(backSights) {
                    return shotCell(indexInChunk + shotOffset, cwSurveyEditorCellIndex::ShotBackClinoCell, 0);
                } else if(frontSights) {
                    return shotCell(indexInChunk + shotOffset, cwSurveyEditorCellIndex::ShotClinoCell, 0);
                }
                return shotCell(indexInChunk + shotOffset, cwSurveyEditorCellIndex::ShotDistanceCell, 0);
            }
        case Down:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::StationLeftCell, 1);
        case Up:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::StationLeftCell, -1);
        }
        break;
    case cwSurveyEditorCellIndex::StationRightCell:
        switch(key) {
        case Tab:
        case Right:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::StationUpCell, 0);
        case BackTab:
        case Left:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::StationLeftCell, 0);
        case Down:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::StationRightCell, 1);
        case Up:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::StationRightCell, -1);
        }
        break;
    case cwSurveyEditorCellIndex::StationUpCell:
        switch(key) {
        case Tab:
        case Right:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::StationDownCell, 0);
        case BackTab:
        case Left:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::StationRightCell, 0);
        case Down:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::StationUpCell, 1);
        case Up:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::StationUpCell, -1);
        }
        break;
    case cwSurveyEditorCellIndex::StationDownCell:
        switch(key) {
        case Tab:
        case Right:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::StationSplaysCell, 0);
        case BackTab:
        case Left:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::StationUpCell, 0);
        case Down:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::StationDownCell, 1);
        case Up:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::StationDownCell, -1);
        }
        break;
    case cwSurveyEditorCellIndex::StationSplaysCell:
        //The last cell in a station row, so it inherits the way D used to leave
        //the row
        switch(key) {
        case Tab: {
            //The cluster the cell has open is the next thing in the chain, and
            //its first reading is where tab lands
            const auto firstSplay = splayCell(0, cwSurveyEditorCellIndex::SplayDistanceCell);
            if(isCellValid(firstSplay)) {
                return firstSplay;
            }
            return tabOutOfStationRow();
        }
        case BackTab:
        case Left:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::StationDownCell, 0);
        case Right:
            return cwSurveyEditorCellIndex();
        case Down:
            //A cluster's readings stand in the distance, compass and clino
            //columns rather than this one, so the arrows run past them and tab
            //is the way in. Diving into the cluster here would give two cells
            //the same Down and leave the first splay's Up with no way back
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::StationSplaysCell, 1);
        case Up:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::StationSplaysCell, -1);
        }
        break;
    case cwSurveyEditorCellIndex::ShotDistanceCell:
        switch(key) {
        case Tab:
            if(frontSights) {
                return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotCompassCell, 0);
            } else if(backSights) {
                return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotBackCompassCell, 0);
            }
            return stationCell(indexInChunk, cwSurveyEditorCellIndex::StationLeftCell, 0);
        case Right:
            if(backSights) {
                return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotBackCompassCell, 0);
            } else if(frontSights) {
                return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotCompassCell, 0);
            }
            return stationCell(indexInChunk + 1, cwSurveyEditorCellIndex::StationLeftCell, 0);
        case BackTab:
        case Left:
            return stationCell(indexInChunk + 1, cwSurveyEditorCellIndex::StationNameCell, 0);
        case Down:
            return shotRowStep(cwSurveyEditorCellIndex::ShotDistanceCell, 1);
        case Up:
            return shotRowStep(cwSurveyEditorCellIndex::ShotDistanceCell, -1);
        }
        break;
    case cwSurveyEditorCellIndex::ShotCompassCell:
        switch(key) {
        case Tab:
            if(backSights) {
                return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotBackCompassCell, 0);
            } else if(frontSights) {
                return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotClinoCell, 0);
            }
            return stationCell(indexInChunk + 1, cwSurveyEditorCellIndex::StationLeftCell, 0);
        case BackTab:
        case Left:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotDistanceCell, 0);
        case Right:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotClinoCell, 0);
        case Down:
            if(backSights) {
                return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotBackCompassCell, 0);
            }
            return shotRowStep(cwSurveyEditorCellIndex::ShotCompassCell, 1);
        case Up:
            if(backSights) {
                return shotRowStep(cwSurveyEditorCellIndex::ShotBackCompassCell, -1);
            }
            return shotRowStep(cwSurveyEditorCellIndex::ShotCompassCell, -1);
        }
        break;
    case cwSurveyEditorCellIndex::ShotBackCompassCell:
        switch(key) {
        case Tab:
            if(frontSights) {
                return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotClinoCell, 0);
            } else if(backSights) {
                return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotBackClinoCell, 0);
            }
            return stationCell(indexInChunk + 1, cwSurveyEditorCellIndex::StationLeftCell, 0);
        case BackTab:
            if(frontSights) {
                return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotCompassCell, 0);
            }
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotDistanceCell, 0);
        case Right:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotBackClinoCell, 0);
        case Left:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotDistanceCell, 0);
        case Down:
            if(frontSights) {
                return shotRowStep(cwSurveyEditorCellIndex::ShotCompassCell, 1);
            }
            return shotRowStep(cwSurveyEditorCellIndex::ShotBackCompassCell, 1);
        case Up:
            if(frontSights) {
                return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotCompassCell, 0);
            }
            return shotRowStep(cwSurveyEditorCellIndex::ShotBackCompassCell, -1);
        }
        break;
    case cwSurveyEditorCellIndex::ShotClinoCell:
        switch(key) {
        case Tab:
            if(backSights) {
                return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotBackClinoCell, 0);
            }
            return nextLeftFromClino();
        case BackTab:
            if(backSights) {
                return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotBackCompassCell, 0);
            } else if(frontSights) {
                return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotCompassCell, 0);
            }
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotDistanceCell, 0);
        case Right:
            return stationCell(indexInChunk + 1, cwSurveyEditorCellIndex::StationLeftCell, 0);
        case Left:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotCompassCell, 0);
        case Down:
            if(backSights) {
                return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotBackClinoCell, 0);
            }
            return shotRowStep(cwSurveyEditorCellIndex::ShotClinoCell, 1);
        case Up:
            if(backSights) {
                return shotRowStep(cwSurveyEditorCellIndex::ShotBackClinoCell, -1);
            }
            return shotRowStep(cwSurveyEditorCellIndex::ShotClinoCell, -1);
        }
        break;
    case cwSurveyEditorCellIndex::ShotBackClinoCell:
        switch(key) {
        case Tab:
            return nextLeftFromClino();
        case BackTab:
            if(frontSights) {
                return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotClinoCell, 0);
            } else if(backSights) {
                return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotBackCompassCell, 0);
            }
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotDistanceCell, 0);
        case Right:
            return stationCell(indexInChunk + 1, cwSurveyEditorCellIndex::StationLeftCell, 0);
        case Left:
            return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotBackCompassCell, 0);
        case Down:
            if(frontSights) {
                return shotRowStep(cwSurveyEditorCellIndex::ShotClinoCell, 1);
            }
            return shotRowStep(cwSurveyEditorCellIndex::ShotBackClinoCell, 1);
        case Up:
            if(frontSights) {
                return offsetCurrentRowRole(cwSurveyEditorCellIndex::ShotClinoCell, 0);
            }
            return shotRowStep(cwSurveyEditorCellIndex::ShotBackClinoCell, -1);
        }
        break;
    case cwSurveyEditorCellIndex::SplayDistanceCell:
    case cwSurveyEditorCellIndex::SplayCompassCell:
    case cwSurveyEditorCellIndex::SplayClinoCell:
        return nextSplayCell(currentCell.cellRole());
    //The included checkbox rides along with the distance cell, so the keyboard
    //never lands on it
    case cwSurveyEditorCellIndex::ShotDistanceIncludedCell:
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
    if(rowIndex.rowType() != cwSurveyEditorRowIndex::StationRow) {
        return;
    }

    cwSurveyChunk* chunk = splayClusterChunk(rowIndex);
    if(chunk == nullptr) {
        return;
    }

    const int stationIndex = rowIndex.indexInChunk();

    if(splayRowCount(chunk, stationIndex) > 0) {
        collapseSplays(chunk, stationIndex);
    } else {
        expandSplays(chunk, stationIndex);
    }
}

/**
 * @brief cwSurveyEditorModel::expandSplays
 *
 * Opens \a stationIndex's cluster: a row for each splay it carries, plus the
 * blank row its next splay is typed into. A station with no splays opens on the
 * blank row alone, which is how manual entry starts. A cluster that is already
 * open keeps the rows it has.
 */
void cwSurveyEditorModel::expandSplays(cwSurveyChunk* chunk, int stationIndex)
{
    if(chunk == nullptr || splayRowCount(chunk, stationIndex) > 0) {
        return;
    }

    const int stationRow = toModelRow({chunk, stationIndex, cwSurveyEditorRowIndex::StationRow});
    if(stationRow < 0) {
        return;
    }

    const int rowsShown = chunk->stationSplayCount(stationIndex) + kBlankSplayRowCount;

    beginInsertRows(QModelIndex(), stationRow + 1, stationRow + rowsShown);
    m_expandedSplays[chunk].insert(stationIndex, rowsShown);
    endInsertRows();

    emitSplayExpansionChanged(chunk, stationIndex);
}

/**
 * @brief cwSurveyEditorModel::splayEntryCell
 * @param stationRowIndex - The station row whose cluster the entry lands in
 * @return The distance cell of the cluster's blank row, or an invalid cell when
 * the station's cluster is closed
 *
 * Where typing a station's next splay starts. The Splays cell uses it to put the
 * caret in the blank row it just opened, so manual entry on a station with no
 * splays is tab, Enter, type.
 */
cwSurveyEditorCellIndex cwSurveyEditorModel::splayEntryCell(const cwSurveyEditorRowIndex& stationRowIndex) const
{
    if(stationRowIndex.rowType() != cwSurveyEditorRowIndex::StationRow) {
        return cwSurveyEditorCellIndex();
    }

    cwSurveyChunk* chunk = splayClusterChunk(stationRowIndex);
    if(chunk == nullptr) {
        return cwSurveyEditorCellIndex();
    }

    const int stationIndex = stationRowIndex.indexInChunk();
    const int blankRow = toModelRow(cwSurveyEditorRowIndex(chunk,
                                                           stationIndex,
                                                           chunk->stationSplayCount(stationIndex),
                                                           cwSurveyEditorRowIndex::SplayRow));
    if(blankRow < 0) {
        return cwSurveyEditorCellIndex();
    }

    return cellIndex(blankRow, cwSurveyEditorCellIndex::SplayDistanceCell);
}

/**
 * @brief cwSurveyEditorModel::splayClusterChunk
 * @param rowIndex - A row that hangs off a station's splay cluster
 * @return The chunk the cluster lives in, or nullptr when \a rowIndex names a
 * station this model isn't showing
 *
 * Both splay mutations and the expansion toggle reach a cluster through a row
 * the view handed back, which can name a chunk the trip has since dropped or a
 * station the chunk has since lost.
 */
cwSurveyChunk* cwSurveyEditorModel::splayClusterChunk(const cwSurveyEditorRowIndex& rowIndex) const
{
    cwSurveyChunk* chunk = rowIndex.chunk();
    const int stationIndex = rowIndex.indexInChunk();

    if(chunk == nullptr
        || m_trip.isNull()
        || !m_trip->chunks().contains(chunk)
        || stationIndex < 0
        || stationIndex >= chunk->stationCount())
    {
        return nullptr;
    }

    return chunk;
}

/**
 * @brief cwSurveyEditorModel::removeSplayAt
 * @param rowIndex - The splay row the user asked to be rid of
 *
 * Removing one splay is immediate: the cluster it came out of is on screen, so
 * a wrong one is cheap to see and to shoot again.
 */
void cwSurveyEditorModel::removeSplayAt(const cwSurveyEditorRowIndex& rowIndex)
{
    if(rowIndex.rowType() != cwSurveyEditorRowIndex::SplayRow) {
        return;
    }

    cwSurveyChunk* chunk = splayClusterChunk(rowIndex);
    if(chunk == nullptr) {
        return;
    }

    chunk->removeStationSplay(rowIndex.indexInChunk(), rowIndex.splayIndex());
}

/**
 * @brief cwSurveyEditorModel::clearSplaysAt
 * @param rowIndex - A station row, or any splay row hanging off it
 *
 * Takes every splay off the station. This is the destructive one, so the UI
 * asks first; the model just does as it's told.
 */
void cwSurveyEditorModel::clearSplaysAt(const cwSurveyEditorRowIndex& rowIndex)
{
    switch(rowIndex.rowType()) {
    case cwSurveyEditorRowIndex::StationRow:
    case cwSurveyEditorRowIndex::SplayRow:
        break;
    case cwSurveyEditorRowIndex::TitleRow:
    case cwSurveyEditorRowIndex::ShotRow:
        return;
    }

    cwSurveyChunk* chunk = splayClusterChunk(rowIndex);
    if(chunk == nullptr) {
        return;
    }

    chunk->clearStationSplays(rowIndex.indexInChunk());
}

/**
 * @brief cwSurveyEditorModel::startSplayMove
 * @param rowIndex - The splay row to move, or the cluster's station row
 * @param allSplays - True moves every splay the station carries, false the one
 * splay \a rowIndex names
 *
 * Arms a move and waits for the user to pick the station it lands on. Arming a
 * move calls off the one before it, so the table only ever has one in flight.
 */
void cwSurveyEditorModel::startSplayMove(const cwSurveyEditorRowIndex& rowIndex, bool allSplays)
{
    cancelSplayMove();

    cwSurveyChunk* chunk = splayClusterChunk(rowIndex);
    if(chunk == nullptr) {
        return;
    }

    const int stationIndex = rowIndex.indexInChunk();
    const int splayCount = chunk->stationSplayCount(stationIndex);

    QList<int> splayIndices;
    if(allSplays) {
        if(rowIndex.rowType() != cwSurveyEditorRowIndex::StationRow
            && rowIndex.rowType() != cwSurveyEditorRowIndex::SplayRow)
        {
            return;
        }

        splayIndices.reserve(splayCount);
        for(int splayIndex = 0; splayIndex < splayCount; ++splayIndex) {
            splayIndices.append(splayIndex);
        }
    } else {
        if(rowIndex.rowType() != cwSurveyEditorRowIndex::SplayRow) {
            return;
        }
        if(rowIndex.splayIndex() < 0 || rowIndex.splayIndex() >= splayCount) {
            return;
        }

        splayIndices.append(rowIndex.splayIndex());
    }

    if(splayIndices.isEmpty()) {
        return;
    }

    m_pendingSplayMove = {chunk, stationIndex, splayIndices};
    emit splayMoveChanged();
}

/**
 * @brief cwSurveyEditorModel::cancelSplayMove
 *
 * Leaves the splays where they are. Every tap that isn't on a station the move
 * can land on comes through here.
 */
void cwSurveyEditorModel::cancelSplayMove()
{
    if(!splayMoveActive()) {
        return;
    }

    m_pendingSplayMove = PendingSplayMove();
    emit splayMoveChanged();
}

/**
 * @brief cwSurveyEditorModel::commitSplayMove
 * @param targetRowIndex - The station row the user picked
 *
 * Moves the armed splays onto \a targetRowIndex's station, leaving that
 * station's cluster as closed as it was — its chip counting the splays it took
 * is the confirmation. A row the move can't land on leaves it armed for the
 * next pick.
 */
void cwSurveyEditorModel::commitSplayMove(const cwSurveyEditorRowIndex& targetRowIndex)
{
    if(!isSplayMoveTarget(targetRowIndex)) {
        return;
    }

    cwSurveyChunk* target = splayClusterChunk(targetRowIndex);
    cwSurveyChunk* source = m_pendingSplayMove.chunk;
    const int sourceStation = m_pendingSplayMove.stationIndex;
    const int targetStation = targetRowIndex.indexInChunk();
    const QList<int> splayIndices = m_pendingSplayMove.splayIndices;

    //The move is over before the chunks announce it, so the rows arriving
    //through stationSplaysChanged are read by a table with nothing in flight
    m_pendingSplayMove = PendingSplayMove();
    emit splayMoveChanged();

    cwSurveyChunk::moveStationSplays(source, sourceStation, target, targetStation, splayIndices);
}

/**
 * @brief cwSurveyEditorModel::isSplayMoveTarget
 * @return True when an armed move can land on \a rowIndex's station
 *
 * Any station in the trip takes the splays, in this chunk or another. The one
 * they came off is the only station excluded — it already has them.
 */
bool cwSurveyEditorModel::isSplayMoveTarget(const cwSurveyEditorRowIndex& rowIndex) const
{
    if(!splayMoveActive() || rowIndex.rowType() != cwSurveyEditorRowIndex::StationRow) {
        return false;
    }

    cwSurveyChunk* chunk = splayClusterChunk(rowIndex);
    if(chunk == nullptr) {
        return false;
    }

    return chunk != m_pendingSplayMove.chunk
           || rowIndex.indexInChunk() != m_pendingSplayMove.stationIndex;
}

/**
 * @brief cwSurveyEditorModel::isSplayMoveSource
 * @return True when \a rowIndex names the station an armed move's splays are
 * leaving
 *
 * Only the rows that hang off a cluster answer: a shot row carries the same
 * index as the station above it, and it is no part of the move.
 */
bool cwSurveyEditorModel::isSplayMoveSource(const cwSurveyEditorRowIndex& rowIndex) const
{
    if(!splayMoveActive()) {
        return false;
    }

    switch(rowIndex.rowType()) {
    case cwSurveyEditorRowIndex::StationRow:
    case cwSurveyEditorRowIndex::SplayRow:
        break;
    case cwSurveyEditorRowIndex::TitleRow:
    case cwSurveyEditorRowIndex::ShotRow:
        return false;
    }

    return splayClusterChunk(rowIndex) == m_pendingSplayMove.chunk
           && rowIndex.indexInChunk() == m_pendingSplayMove.stationIndex;
}

bool cwSurveyEditorModel::splayMoveActive() const
{
    return !m_pendingSplayMove.chunk.isNull() && !m_pendingSplayMove.splayIndices.isEmpty();
}

int cwSurveyEditorModel::splayMoveCount() const
{
    return splayMoveActive() ? m_pendingSplayMove.splayIndices.size() : 0;
}

//! The name of the station an armed move's splays are leaving, for the banner
QString cwSurveyEditorModel::splayMoveStationName() const
{
    if(!splayMoveActive()) {
        return QString();
    }

    return m_pendingSplayMove.chunk->data(cwSurveyChunk::StationNameRole,
                                          m_pendingSplayMove.stationIndex).toString();
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
    const int rowsWanted = splayCount + kBlankSplayRowCount;

    //A closed cluster only moves the chip's count; an open one grows or shrinks
    //to match, and the station row itself stays put either way. A cluster the
    //last splay leaves closes, blank row and all — there's nothing left to look
    //at, and the Splays cell opens it again for the next entry
    if(shownRows > 0) {
        if(splayCount == 0) {
            collapseSplays(chunk, stationIndex);
        } else if(rowsWanted > shownRows) {
            beginInsertRows(QModelIndex(),
                            stationRow + 1 + shownRows,
                            stationRow + rowsWanted);
            m_expandedSplays[chunk].insert(stationIndex, rowsWanted);
            endInsertRows();
        } else if(rowsWanted < shownRows) {
            beginRemoveRows(QModelIndex(),
                            stationRow + 1 + rowsWanted,
                            stationRow + shownRows);
            m_expandedSplays[chunk].insert(stationIndex, rowsWanted);
            endRemoveRows();
        }
    }

    emit dataChanged(index(stationRow), index(stationRow), {StationSplayCountRole});

    const int rowsLeft = splayRowCount(chunk, stationIndex);
    if(rowsLeft > 0) {
        //IsVirtualRole rides along because the row that was blank holds a splay
        //once one is typed into it, and the fresh blank row takes its place
        emit dataChanged(index(stationRow + 1), index(stationRow + rowsLeft),
                         {SplayDistanceRole, SplayCompassRole, SplayClinoRole,
                          StationSplayCountRole, IsVirtualRole});
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

cwSurveyEditorRowIndex::RowType cwSurveyEditorModel::toRowType(cwSurveyEditorCellIndex::CellRole cellRole) const
{
    switch(cellRole) {
    case cwSurveyEditorCellIndex::StationNameCell:
    case cwSurveyEditorCellIndex::StationLeftCell:
    case cwSurveyEditorCellIndex::StationRightCell:
    case cwSurveyEditorCellIndex::StationUpCell:
    case cwSurveyEditorCellIndex::StationDownCell:
    case cwSurveyEditorCellIndex::StationSplaysCell:
        return cwSurveyEditorRowIndex::StationRow;
    case cwSurveyEditorCellIndex::ShotDistanceCell:
    case cwSurveyEditorCellIndex::ShotDistanceIncludedCell:
    case cwSurveyEditorCellIndex::ShotCompassCell:
    case cwSurveyEditorCellIndex::ShotBackCompassCell:
    case cwSurveyEditorCellIndex::ShotClinoCell:
    case cwSurveyEditorCellIndex::ShotBackClinoCell:
        return cwSurveyEditorRowIndex::ShotRow;
    case cwSurveyEditorCellIndex::SplayDistanceCell:
    case cwSurveyEditorCellIndex::SplayCompassCell:
    case cwSurveyEditorCellIndex::SplayClinoCell:
        return cwSurveyEditorRowIndex::SplayRow;
    }

    //Only a default constructed cell reaches here
    Q_ASSERT(false);
    return cwSurveyEditorRowIndex::TitleRow;
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
 * The number of splay rows station \a stationIndex is currently showing — its
 * splays and the blank row under them — which is zero unless its cluster is
 * open.
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
        m_focusedCellRole = static_cast<cwSurveyEditorCellIndex::CellRole>(-1);
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
