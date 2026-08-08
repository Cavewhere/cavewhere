/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwRecenterCandidateModel.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwLocalProjectionManager.h"

cwRecenterCandidateModel::cwRecenterCandidateModel(cwLocalProjectionManager* manager,
                                                   cwCavingRegion* region) :
    QAbstractListModel(manager),
    m_manager(manager),
    m_region(region)
{
    Q_ASSERT(m_manager != nullptr);
    Q_ASSERT(m_region != nullptr);

    refresh();
}

int cwRecenterCandidateModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return count();
}

QVariant cwRecenterCandidateModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() >= m_candidates.size()) {
        return QVariant();
    }

    const cwRecenterCandidate& candidate = m_candidates.at(index.row());
    switch (role) {
    case StationIdRole:
        return candidate.stationId;
    case StationNameRole:
        return candidate.stationName;
    case CaveNameRole:
        return candidate.caveName;
    case EligibleRole:
        return candidate.eligible;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> cwRecenterCandidateModel::roleNames() const
{
    return {
        {StationIdRole,   "stationId"},
        {StationNameRole, "stationName"},
        {CaveNameRole,    "caveName"},
        {EligibleRole,    "eligible"}
    };
}

void cwRecenterCandidateModel::refresh()
{
    const auto center = m_manager->dataCenter();
    if (!center.has_value()) {
        // No frame, or nothing in the project the frame can place. Recentering
        // re-derives a frame that exists and is never how the first one gets
        // made, so there is nothing to offer.
        setCandidates({});
        return;
    }

    QList<cwRecenterCandidate> candidates;
    for (cwCave* cave : m_region->caves()) {
        if (cave == nullptr) {
            continue;
        }
        for (const cwFixStation& fix : cave->fixStations()->fixStations()) {
            const auto local = m_manager->localPointOfFix(fix);
            if (!local.has_value()) {
                continue;
            }
            // Centering moves the origin onto the station, so the distance from
            // the station to the middle of the data is where the data would end
            // up relative to the new origin. Deriving the candidate frame and
            // re-transforming every input per station would answer the same
            // question to within the tmerc distortion over these distances —
            // ~11 ppm at 30 km, noise against the frame's reach.
            candidates.append(cwRecenterCandidate{
                fix.id(),
                fix.stationName(),
                cave->name(),
                cwLocalProjectionManager::isWithinReach(*center, *local)
            });
        }
    }

    setCandidates(candidates);
}

void cwRecenterCandidateModel::setCandidates(const QList<cwRecenterCandidate>& candidates)
{
    const int previousCount = count();

    beginResetModel();
    m_candidates = candidates;
    endResetModel();

    if (previousCount != count()) {
        emit countChanged();
    }
}
