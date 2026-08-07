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
//! cwTrip::knownStations() owns which sources answer that and how they merge —
//! this surface offers a tie between two stations, so it needs every station
//! either side is known to have, whether or not the solve placed it.
QList<cwStationHandle> localStations(const cwTrip* trip)
{
    const QList<cwTrip::KnownStation> known = trip->knownStations();

    QList<cwStationHandle> stations;
    stations.reserve(known.size());
    for (const cwTrip::KnownStation& station : known) {
        stations.append(trip->stationHandle(station.name));
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
    //Every pulse that can move a suggestion, named in one place so connect and
    //disconnect cannot drift apart. This model reads every trip in the cave, so
    //it needs cave-wide coverage of the two sources that move: the solve lands
    //in the cave's lookup for all trips at once, so the bound trip's
    //solvedStationsChanged speaks for its siblings' solves too, while a harvest
    //is one trip's own field and only cwCave::tripExternalStationsChanged
    //carries a sibling's. Subscribing to the bound trip's knownStationsChanged
    //instead of its solve would rebuild twice for its own harvest, which the
    //cave's aggregate already delivers. scopeLabelsChanged is the region's
    //aggregate rename: a renamed trip is a row's label, and an added or removed
    //one is a whole candidate list.
    //
    //The cave and region senders are remembered at connect time: by disconnect
    //time the trip may be destroyed (the QPointer already null) or removed from
    //its cave, and re-deriving the senders from it would leave their
    //connections behind to rebuild once per bind for the model's lifetime.
    if (connected) {
        m_connectedCave = trip != nullptr ? trip->parentCave() : nullptr;
        m_connectedRegion = m_connectedCave != nullptr
                ? m_connectedCave->parentRegion() : nullptr;
    }

    cwCave* cave = m_connectedCave.data();
    cwCavingRegion* region = m_connectedRegion.data();

    const auto forEachSignal = [trip, cave, region](auto apply) {
        if (trip != nullptr) {
            apply(trip, &cwTrip::solvedStationsChanged);
            apply(trip, &cwTrip::scopeChanged);
        }
        if (cave != nullptr) {
            apply(cave, &cwCave::tripExternalStationsChanged);
        }
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
        m_connectedCave = nullptr;
        m_connectedRegion = nullptr;
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

        //Already in natural order — cwTrip::knownStations() owns that, and this
        //list comes from one trip, unlike the candidates merged above.
        const QList<cwStationHandle> stations = localStations(m_trip);

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
