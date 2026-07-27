//Our includes
#include "cwScopeLabels.h"
#include "cwCavernNaming.h"
#include "cwCaveData.h"
#include "cwCavingRegionData.h"
#include "cwTripData.h"

namespace {

//! Answered for a cave the pool never saw, so tripLabels() can hand back a
//! reference without the caller checking first.
const QHash<QUuid, QString> kNoTripLabels;

QList<cwCavernNaming::ScopeEntry> tripEntries(const QList<cwTripData>& trips)
{
    QList<cwCavernNaming::ScopeEntry> entries;
    entries.reserve(trips.size());
    for (const cwTripData& trip : trips) {
        entries.append({trip.id, trip.name});
    }
    return entries;
}

}

cwScopeLabels::cwScopeLabels(const cwCavingRegionData& region)
{
    QList<cwCavernNaming::ScopeEntry> caveEntries;
    caveEntries.reserve(region.caves.size());
    for (const cwCaveData& cave : region.caves) {
        caveEntries.append({cave.id, cave.name});
    }

    m_caveLabels = cwCavernNaming::scopeLabels(caveEntries);

    m_caveIdsByLabel.reserve(m_caveLabels.size());
    for (auto iter = m_caveLabels.constBegin(); iter != m_caveLabels.constEnd(); ++iter) {
        m_caveIdsByLabel.insert(iter.value(), iter.key());
    }

    m_tripLabelsByCave.reserve(region.caves.size());
    for (const cwCaveData& cave : region.caves) {
        m_tripLabelsByCave.insert(cave.id, cwCavernNaming::scopeLabels(tripEntries(cave.trips)));
    }
}

cwScopeLabels cwScopeLabels::forCave(const cwCaveData& cave)
{
    cwScopeLabels labels;
    labels.m_tripLabelsByCave.insert(cave.id, cwCavernNaming::scopeLabels(tripEntries(cave.trips)));
    return labels;
}

QString cwScopeLabels::caveLabel(const QUuid& caveId) const
{
    return m_caveLabels.value(caveId);
}

QString cwScopeLabels::cavePrefix(const QUuid& caveId) const
{
    return cwCavernNaming::scopePrefix(caveLabel(caveId));
}

QUuid cwScopeLabels::caveId(const QString& caveLabel) const
{
    return m_caveIdsByLabel.value(caveLabel);
}

const QHash<QUuid, QString>& cwScopeLabels::tripLabels(const QUuid& caveId) const
{
    const auto iter = m_tripLabelsByCave.constFind(caveId);
    if (iter == m_tripLabelsByCave.constEnd()) {
        return kNoTripLabels;
    }
    return iter.value();
}
