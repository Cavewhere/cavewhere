/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwTieSuggestionModel.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwNameUtils.h"
#include "cwStation.h"
#include "cwTrip.h"

//Qt includes
#include <QHash>
#include <QVector3D>

//Std includes
#include <algorithm>
#include <utility>

namespace {

//A person picks from a short list or gives up; a long one is the same as none.
constexpr int kMaxSuggestions = 10;

//How many partners one floating station may offer. Enough to disambiguate the
//case the plan names — the same bare name living in several trips — without one
//prolific name crowding out every other station's suggestion.
constexpr int kMaxCandidatesPerStation = 5;

//! The last dotted component of a station's tail, canonicalized. An attached
//! centerline's tail keeps the "*begin" nesting inside its file ("simple.a1")
//! while the native station beside it is bare ("a1"), so the leaf is the only
//! part of the two that can be compared at all.
QString leafKey(const QString& tail)
{
    const int dot = tail.lastIndexOf(QLatin1Char('.'));
    return cwStation::canonicalKey(dot < 0 ? tail : tail.mid(dot + 1));
}

//! The leading letters of a station name — "a" of "a12", "sump" of "sump3".
//! Empty when the name does not start with one, which is not a match anybody
//! wants: every purely numeric station in the cave would share it.
QString letterKey(const QString& leaf)
{
    int end = 0;
    while (end < leaf.size() && leaf.at(end).isLetter()) {
        ++end;
    }
    return leaf.left(end);
}

//! A trip's stations in its own namespace, as the identities a tie stores.
//!
//! Chunks first, and a file-backed trip's names only for a trip that has none:
//! an unconnected native chunk stops the solve outright, so the very trips the
//! native half of this surface is for would have no solved station to offer.
//! An attached centerline is the other way round — its stations live in its
//! file, so they are known only once something has read it.
//!
//! Of the two readers, the scan's harvest is preferred: it reads the attachment
//! on its own, so it has names even for the attachment cavern dropped — which
//! is every attachment this surface is trying to tie in. The solve is the
//! fallback, for the window before the first scan applies.
QList<cwStationHandle> localStations(const cwTrip* trip)
{
    QList<cwStationHandle> stations;

    if (trip->chunkCount() > 0) {
        const QList<cwStation> unique = trip->uniqueStations();
        stations.reserve(unique.size());
        for (const cwStation& station : unique) {
            if (!station.name().isEmpty()) {
                stations.append(trip->stationHandle(station.name()));
            }
        }
        return stations;
    }

    const QStringList harvested = trip->externalStations();
    if (!harvested.isEmpty()) {
        stations.reserve(harvested.size());
        for (const QString& name : harvested) {
            stations.append(trip->stationHandle(name));
        }
        return stations;
    }

    const QList<QPair<QString, QVector3D>> solved = trip->solvedStations();
    stations.reserve(solved.size());
    for (const QPair<QString, QVector3D>& station : solved) {
        stations.append(trip->stationHandle(station.first));
    }

    return stations;
}

} // namespace

cwTieSuggestionModel::cwTieSuggestionModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int cwTieSuggestionModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_rows.size();
}

QVariant cwTieSuggestionModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return QVariant();
    }

    const Row& row = m_rows.at(index.row());
    switch (role) {
    case StationRole:
        return row.station.tail();
    case CandidateStationRole:
        return row.candidate.tail();
    case CandidateTripNameRole:
        return row.candidateTripName;
    case MatchRole:
        return QVariant::fromValue(row.match);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> cwTieSuggestionModel::roleNames() const
{
    return {
        { StationRole, QByteArrayLiteral("station") },
        { CandidateStationRole, QByteArrayLiteral("candidateStation") },
        { CandidateTripNameRole, QByteArrayLiteral("candidateTripName") },
        { MatchRole, QByteArrayLiteral("match") }
    };
}

void cwTieSuggestionModel::setTrip(cwTrip* trip)
{
    if (m_trip == trip) {
        return;
    }

    setTripConnected(m_trip, false);
    m_trip = trip;
    setTripConnected(m_trip, true);

    emit tripChanged();
    rebuildRows();
}

void cwTieSuggestionModel::setTripConnected(cwTrip* trip, bool connected)
{
    if (trip == nullptr) {
        return;
    }

    //Every pulse that can move a suggestion, named in one place so connect and
    //disconnect cannot drift apart. solvedStationsChanged is the cave's, relayed
    //to each of its trips, so a candidate trip gaining or losing a station
    //arrives here too. scopeLabelsChanged is the region's aggregate rename: a
    //renamed trip is a row's label, and an added or removed one is a whole
    //candidate list. externalStationsChanged is the scan's harvest landing,
    //which for a dropped attachment is the only pulse that brings its names.
    cwCave* cave = trip->parentCave();
    cwCavingRegion* region = cave != nullptr ? cave->parentRegion() : nullptr;

    const auto forEachSignal = [trip, region](auto apply) {
        apply(trip, &cwTrip::solvedStationsChanged);
        apply(trip, &cwTrip::externalStationsChanged);
        apply(trip, &cwTrip::scopeChanged);
        if (region != nullptr) {
            apply(region, &cwCavingRegion::scopeLabelsChanged);
        }
    };

    if (connected) {
        forEachSignal([this](auto* sender, auto signal) {
            QObject::connect(sender, signal, this, &cwTieSuggestionModel::rebuildRows);
        });
    } else {
        forEachSignal([this](auto* sender, auto signal) {
            QObject::disconnect(sender, signal, this, &cwTieSuggestionModel::rebuildRows);
        });
    }
}

bool cwTieSuggestionModel::tieAt(int row)
{
    if (row < 0 || row >= m_rows.size() || m_trip.isNull()) {
        return false;
    }

    const cwCave* cave = m_trip->parentCave();
    cwCavingRegion* region = cave != nullptr ? cave->parentRegion() : nullptr;
    if (region == nullptr) {
        return false;
    }

    const Row& suggestion = m_rows.at(row);
    return region->tieStations(suggestion.station, suggestion.candidate);
}

void cwTieSuggestionModel::rebuildRows()
{
    const int oldCount = m_rows.size();
    const bool wasTruncated = m_truncated;

    beginResetModel();
    m_rows.clear();
    m_truncated = false;

    const cwCave* cave = m_trip.isNull() ? nullptr : m_trip->parentCave();
    if (cave != nullptr) {
        struct Candidate {
            cwStationHandle handle;
            QString tripName;
        };

        QList<Candidate> candidates;
        for (const cwTrip* other : cave->trips()) {
            if (other == nullptr || other == m_trip.data()) {
                continue;
            }
            for (const cwStationHandle& station : localStations(other)) {
                candidates.append({ station, other->name() });
            }
        }

        //Sorted once, up front, so every bucket below comes out in reading order
        //and the rows stay deterministic even though they are cut off early.
        std::stable_sort(candidates.begin(), candidates.end(),
                         [](const Candidate& left, const Candidate& right) {
            return cwNameUtils::naturalLess(left.handle.tail(), right.handle.tail());
        });

        QHash<QString, QList<int>> byLeaf;
        QHash<QString, QList<int>> byLetter;
        for (int i = 0; i < candidates.size(); ++i) {
            const QString leaf = leafKey(candidates.at(i).handle.tail());
            byLeaf[leaf].append(i);

            const QString letters = letterKey(leaf);
            if (!letters.isEmpty()) {
                byLetter[letters].append(i);
            }
        }

        QList<cwStationHandle> stations = localStations(m_trip);
        std::stable_sort(stations.begin(), stations.end(),
                         [](const cwStationHandle& left, const cwStationHandle& right) {
            return cwNameUtils::naturalLess(left.tail(), right.tail());
        });

        //Two passes rather than one sort: a station named alike anywhere in the
        //cave outranks every merely similar name, so the near matches cannot be
        //collected until the whole cave has been asked for exact ones.
        const auto collect = [&](Match match, const QHash<QString, QList<int>>& index,
                                 auto keyOf, auto accepts) {
            for (const cwStationHandle& station : std::as_const(stations)) {
                if (!accepts(station)) {
                    continue;
                }

                const auto found = index.constFind(keyOf(station));
                if (found == index.constEnd()) {
                    continue;
                }

                int taken = 0;
                for (int candidate : found.value()) {
                    if (taken >= kMaxCandidatesPerStation) {
                        break;
                    }
                    if (m_rows.size() >= kMaxSuggestions) {
                        m_truncated = true;
                        return;
                    }
                    m_rows.append({ station,
                                    candidates.at(candidate).handle,
                                    candidates.at(candidate).tripName,
                                    match });
                    ++taken;
                }
            }
        };

        const auto leafOf = [](const cwStationHandle& station) {
            return leafKey(station.tail());
        };

        collect(SameName, byLeaf, leafOf, [](const cwStationHandle&) { return true; });

        //Only for a station nothing in the cave is named after: a near match is
        //a guess, and offering one beside the exact answer for the same station
        //reads as though the two were equally likely.
        collect(SameLetterPrefix, byLetter,
                [&leafOf](const cwStationHandle& station) {
                    return letterKey(leafOf(station));
                },
                [&](const cwStationHandle& station) {
                    return !byLeaf.contains(leafOf(station))
                            && !letterKey(leafOf(station)).isEmpty();
                });
    }

    endResetModel();

    if (m_rows.size() != oldCount || m_truncated != wasTruncated) {
        emit countChanged();
    }
}
