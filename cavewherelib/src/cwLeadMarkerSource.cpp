/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLeadMarkerSource.h"
#include "cwRegionTreeModel.h"
#include "cwScrap.h"

namespace {
// Pulls the lead marker toward the viewer so the scrap and centerline geometry
// near the lead don't draw over it. Occlusion picking reuses it as its slack, so
// a tap is rejected only when geometry is more than this far in front of the
// lead, which is when the drawn marker is hidden too.
constexpr float kLeadDepthBias = 1.0f;

// The lead marker is centered on its lead.
constexpr QPointF kLeadAnchor(0.5, 0.5);
}

cwLeadMarkerSource::cwLeadMarkerSource(QObject* parent) :
    cwMarkerSource(parent)
{
}

cwRegionTreeModel* cwLeadMarkerSource::regionModel() const
{
    return m_regionModel;
}

void cwLeadMarkerSource::setRegionModel(cwRegionTreeModel* regionModel)
{
    if(m_regionModel == regionModel) {
        return;
    }

    if(!m_regionModel.isNull()) {
        disconnect(m_regionModel, nullptr, this, nullptr);
    }
    clearAllScraps();

    m_regionModel = regionModel;

    if(!m_regionModel.isNull()) {
        connect(m_regionModel, &QAbstractItemModel::rowsInserted, this, &cwLeadMarkerSource::scrapsInserted);
        connect(m_regionModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, &cwLeadMarkerSource::scrapsAboutToBeRemoved);
        connect(m_regionModel, &QAbstractItemModel::modelAboutToBeReset, this, &cwLeadMarkerSource::clearAllScraps);
        connect(m_regionModel, &QAbstractItemModel::modelReset, this, &cwLeadMarkerSource::addAllScraps);
        addAllScraps();
    }

    emit regionModelChanged();
}

int cwLeadMarkerSource::row(cwScrap* scrap, int leadIndex) const
{
    const int index = scrapIndex(scrap);
    if(index < 0 || leadIndex < 0 || leadIndex >= m_scraps.at(index).leadCount) {
        return -1;
    }
    return firstRow(index) + leadIndex;
}

QPointF cwLeadMarkerSource::spriteAnchor() const
{
    return kLeadAnchor;
}

float cwLeadMarkerSource::depthBias() const
{
    return kLeadDepthBias;
}

void cwLeadMarkerSource::clearAllScraps()
{
    for(const ScrapEntry& entry : std::as_const(m_scraps)) {
        disconnect(entry.scrap, nullptr, this, nullptr);
    }
    m_scraps.clear();
    m_currentScrapId = 0;
    if(count() > 0) {
        resetMarkers({});
    }
}

/**
 * Tracks every scrap of the region model and announces all their leads as one reset.
 */
void cwLeadMarkerSource::addAllScraps()
{
    if(m_regionModel.isNull()) {
        return;
    }

    // Scrap ids restart, so a reloaded project names its LeadPoints as before.
    m_currentScrapId = 0;

    QList<cwMarker> markers;
    const QList<cwScrap*> scraps = m_regionModel->all<cwScrap*>(QModelIndex(), &cwRegionTreeModel::scrap);
    for(cwScrap* scrap : scraps) {
        // A scrap is never tracked twice: a model reset clears them all first (issue #576).
        Q_ASSERT(scrapIndex(scrap) < 0);
        const ScrapEntry entry = trackScrap(scrap);
        markers.append(leadMarkers(entry, 0, entry.leadCount - 1));
    }
    resetMarkers(markers);
}

// Appends @a scrap to the tracked scraps under the next scrap id.
cwLeadMarkerSource::ScrapEntry cwLeadMarkerSource::trackScrap(cwScrap* scrap)
{
    ScrapEntry entry;
    entry.scrap = scrap;
    entry.scrapLeadId = ++m_currentScrapId;
    entry.leadCount = scrap->numberOfLeads();
    m_scraps.append(entry);
    connectScrap(scrap);
    return entry;
}

void cwLeadMarkerSource::addScrap(cwScrap* scrap)
{
    Q_ASSERT(scrap != nullptr);
    Q_ASSERT(scrapIndex(scrap) < 0);

    const int first = count();
    const ScrapEntry entry = trackScrap(scrap);
    insertMarkers(first, leadMarkers(entry, 0, entry.leadCount - 1));
}

void cwLeadMarkerSource::removeScrap(cwScrap* scrap)
{
    const int index = scrapIndex(scrap);
    if(index < 0) {
        return;
    }

    disconnect(scrap, nullptr, this, nullptr);
    const int first = firstRow(index);
    const int leadCount = m_scraps.at(index).leadCount;
    m_scraps.removeAt(index);
    if(m_scraps.isEmpty()) {
        // Scrap ids restart, so the QML tests' LeadPoint names stay stable.
        m_currentScrapId = 0;
    }
    removeMarkers(first, first + leadCount - 1);
}

void cwLeadMarkerSource::connectScrap(cwScrap* scrap)
{
    connect(scrap, &cwScrap::leadsInserted, this, &cwLeadMarkerSource::leadsInserted);
    connect(scrap, &cwScrap::leadsRemoved, this, &cwLeadMarkerSource::leadsRemoved);
    connect(scrap, &cwScrap::leadsDataChanged, this, &cwLeadMarkerSource::leadsDataChanged);
    connect(scrap, &cwScrap::leadsReset, this, &cwLeadMarkerSource::leadsReset);
}

int cwLeadMarkerSource::scrapIndex(const cwScrap* scrap) const
{
    for(int i = 0; i < m_scraps.size(); ++i) {
        if(m_scraps.at(i).scrap == scrap) {
            return i;
        }
    }
    return -1;
}

int cwLeadMarkerSource::firstRow(int scrapIndex) const
{
    int row = 0;
    for(int i = 0; i < scrapIndex; ++i) {
        row += m_scraps.at(i).leadCount;
    }
    return row;
}

/**
 * The markers of @a entry's leads @a first to @a last: each at its lead's
 * position, sharing its scrap's keyword item, and handing its LeadPoint the
 * scrap, the lead's index and the scrap's id.
 */
QList<cwMarker> cwLeadMarkerSource::leadMarkers(const ScrapEntry& entry, int first, int last) const
{
    QList<cwMarker> markers;
    if(first > last) {
        return markers;
    }

    markers.reserve(last - first + 1);
    const QString group = QString::number(entry.scrapLeadId);
    for(int i = first; i <= last; ++i) {
        cwMarker marker;
        marker.position = entry.scrap->leadData(cwScrap::LeadPosition, i).value<QVector3D>();
        marker.spriteIndex = OpenSprite;
        // Keyed by scrap id, since loads can repeat a scrap's UUID.
        marker.visibilityGroup = group;
        marker.keywordExtension = entry.scrap->leadKeywordModel();
        marker.delegateProperties = {{QStringLiteral("scrap"), QVariant::fromValue(entry.scrap)},
                                     {QStringLiteral("pointIndex"), i},
                                     {QStringLiteral("scrapId"), entry.scrapLeadId}};
        markers.append(marker);
    }
    return markers;
}

QList<cwScrap*> cwLeadMarkerSource::scrapsUnder(const QModelIndex& parent, int first, int last) const
{
    QList<cwScrap*> scraps;
    if(m_regionModel.isNull() || !m_regionModel->isNote(parent)) {
        return scraps;
    }
    for(int i = first; i <= last; ++i) {
        const QModelIndex index = m_regionModel->index(i, 0, parent);
        if(auto scrap = m_regionModel->data(index, cwRegionTreeModel::ObjectRole).value<cwScrap*>()) {
            scraps.append(scrap);
        }
    }
    return scraps;
}

void cwLeadMarkerSource::scrapsInserted(const QModelIndex& parent, int first, int last)
{
    const QList<cwScrap*> scraps = scrapsUnder(parent, first, last);
    for(cwScrap* scrap : scraps) {
        addScrap(scrap);
    }
}

void cwLeadMarkerSource::scrapsAboutToBeRemoved(const QModelIndex& parent, int first, int last)
{
    const QList<cwScrap*> scraps = scrapsUnder(parent, first, last);
    for(cwScrap* scrap : scraps) {
        removeScrap(scrap);
    }
}

void cwLeadMarkerSource::leadsInserted(int first, int last)
{
    const int index = scrapIndex(qobject_cast<cwScrap*>(sender()));
    if(index < 0 || first > last) {
        return;
    }

    ScrapEntry& entry = m_scraps[index];
    const int oldCount = entry.leadCount;
    entry.leadCount += last - first + 1;
    const int scrapRow = firstRow(index);
    insertMarkers(scrapRow + first, leadMarkers(entry, first, last));

    // The leads after the inserted ones moved to a higher index.
    if(last + 1 < entry.leadCount && first < oldCount) {
        replaceMarkers(scrapRow + last + 1, leadMarkers(entry, last + 1, entry.leadCount - 1));
    }
}

void cwLeadMarkerSource::leadsRemoved(int first, int last)
{
    const int index = scrapIndex(qobject_cast<cwScrap*>(sender()));
    if(index < 0 || first > last || first < 0 || last >= m_scraps.at(index).leadCount) {
        return;
    }

    ScrapEntry& entry = m_scraps[index];
    entry.leadCount -= last - first + 1;
    const int scrapRow = firstRow(index);
    removeMarkers(scrapRow + first, scrapRow + last);

    // The leads after the removed ones moved to a lower index.
    if(first < entry.leadCount) {
        replaceMarkers(scrapRow + first, leadMarkers(entry, first, entry.leadCount - 1));
    }
}

void cwLeadMarkerSource::leadsDataChanged(int first, int last, const QList<int>& roles)
{
    if(!roles.contains(cwScrap::LeadPosition)) {
        return;
    }

    const int index = scrapIndex(qobject_cast<cwScrap*>(sender()));
    if(index < 0 || first > last) {
        return;
    }

    const ScrapEntry& entry = m_scraps.at(index);
    replaceMarkers(firstRow(index) + first, leadMarkers(entry, first, last));
}

/**
 * Matches the scrap's rows to its new leads: rows both share are updated in
 * place, so their LeadPoints (and an open popup) survive, and the difference
 * is inserted or removed at the scrap's end.
 */
void cwLeadMarkerSource::leadsReset()
{
    const int index = scrapIndex(qobject_cast<cwScrap*>(sender()));
    if(index < 0) {
        return;
    }

    ScrapEntry& entry = m_scraps[index];
    const int oldCount = entry.leadCount;
    const int newCount = entry.scrap->numberOfLeads();
    const int scrapRow = firstRow(index);
    entry.leadCount = newCount;

    const int shared = std::min(oldCount, newCount);
    if(newCount < oldCount) {
        removeMarkers(scrapRow + newCount, scrapRow + oldCount - 1);
    } else if(newCount > oldCount) {
        insertMarkers(scrapRow + oldCount, leadMarkers(entry, oldCount, newCount - 1));
    }
    if(shared > 0) {
        replaceMarkers(scrapRow, leadMarkers(entry, 0, shared - 1));
    }
}
