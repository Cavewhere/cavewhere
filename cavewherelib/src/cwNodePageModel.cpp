/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwNodePageModel.h"
#include "cwCavingRegion.h"
#include "cwExternalSourceModel.h"
#include "cwSurveyNode.h"
#include "cwSurveyTreeModel.h"
#include "cwTrip.h"

namespace {

//The name a hidden item shows for \a sourcePath, excluded from the source whose
//root is named \a rootName: its last path segment, and for a node's own unit
//the node's segment, or the root's name for the root's own unit.
QString hiddenName(const QString& sourcePath, const QString& rootName)
{
    const QStringList segments = sourcePath.split(QLatin1Char('/'));
    if(segments.last() != cwTrip::ownUnitSourcePath()) {
        return segments.last();
    }
    return segments.size() > 1 ? segments.at(segments.size() - 2) : rootName;
}

}

cwNodePageModel::cwNodePageModel(QObject* parent) :
    QObject(parent)
{
}

void cwNodePageModel::setNode(cwSurveyNode* node)
{
    if(m_node == node) {
        return;
    }

    disconnect(m_subtreeConnection);
    disconnect(m_kindConnection);
    disconnect(m_sectionConnection);
    disconnect(m_destroyedConnection);
    disconnectSource();
    m_node = node;

    if(m_node != nullptr) {
        m_subtreeConnection = connect(m_node, &cwSurveyNode::subtreeChanged,
                                      this, &cwNodePageModel::rewireTrips);
        m_kindConnection = connect(m_node, &cwSurveyNode::kindChanged,
                                   this, &cwNodePageModel::kindLabelChanged);
        m_sectionConnection = connect(m_node, &cwSurveyNode::isSectionChanged,
                                      this, &cwNodePageModel::kindLabelChanged);
        //The QPointer is already null here, so the rewire reads an empty node.
        m_destroyedConnection = connect(m_node, &QObject::destroyed, this, [this]() {
            rewireTrips();
            rewireSource();
            emit kindLabelChanged();
            emit nodeChanged();
        });
    }

    rewireTrips();
    rewireSource();
    emit kindLabelChanged();
    emit nodeChanged();
}

QString cwNodePageModel::kindLabel() const
{
    return m_node != nullptr ? cwSurveyTreeModel::kindLabel(m_node.data()) : QString();
}

void cwNodePageModel::rewireTrips()
{
    disconnectTrips();

    if(m_node == nullptr) {
        setTripCount(0);
        setLastSurvey(QDateTime());
        return;
    }

    const QList<cwTrip*> trips = m_node->allTrips();
    m_tripConnections.reserve(trips.size());
    for(cwTrip* trip : trips) {
        m_tripConnections.append(connect(trip, &cwTrip::dateChanged,
                                         this, &cwNodePageModel::updateLastSurvey));
    }

    setTripCount(trips.size());
    updateLastSurvey();
}

void cwNodePageModel::updateLastSurvey()
{
    QDateTime latest;
    if(m_node != nullptr) {
        for(const cwTrip* trip : m_node->allTrips()) {
            const QDateTime date = trip->date();
            if(date.isValid() && (!latest.isValid() || date > latest)) {
                latest = date;
            }
        }
    }
    setLastSurvey(latest);
}

void cwNodePageModel::disconnectTrips()
{
    for(const auto& connection : std::as_const(m_tripConnections)) {
        disconnect(connection);
    }
    m_tripConnections.clear();
}

void cwNodePageModel::setTripCount(int count)
{
    if(m_tripCount == count) {
        return;
    }
    m_tripCount = count;
    emit tripCountChanged();
}

void cwNodePageModel::setLastSurvey(const QDateTime& date)
{
    if(m_lastSurvey == date) {
        return;
    }
    m_lastSurvey = date;
    emit lastSurveyChanged();
}

void cwNodePageModel::disconnectSource()
{
    for(const auto& connection : std::as_const(m_sourceConnections)) {
        disconnect(connection);
    }
    m_sourceConnections.clear();
}

void cwNodePageModel::rewireSource()
{
    disconnectSource();

    if(m_node != nullptr) {
        m_sourceConnections.append(connect(m_node, &cwSurveyNode::sourceChanged,
                                           this, &cwNodePageModel::rewireSource));
        m_sourceConnections.append(connect(m_node, &cwSurveyNode::scopedPositionsChanged,
                                           this, &cwNodePageModel::updateStationCount));

        //Only a root lists what its source hides.
        const cwCavingRegion* region = m_node->parentRegion();
        if(m_node->isSourceRoot() && region != nullptr) {
            const cwExternalSourceModel* sources = region->externalSources();
            const auto update = &cwNodePageModel::updateHiddenNames;
            m_sourceConnections.append(connect(sources, &QAbstractItemModel::dataChanged, this, update));
            m_sourceConnections.append(connect(sources, &QAbstractItemModel::rowsInserted, this, update));
            m_sourceConnections.append(connect(sources, &QAbstractItemModel::rowsRemoved, this, update));
            m_sourceConnections.append(connect(sources, &QAbstractItemModel::modelReset, this, update));
            m_sourceConnections.append(connect(m_node, &cwSurveyNode::nameChanged, this, update));
        }
    }

    updateStationCount();
    updateHiddenNames();
}

void cwNodePageModel::updateStationCount()
{
    const int count = m_node != nullptr && m_node->isSourced()
            ? cwSurveyTreeModel::sourcedStationCount(m_node)
            : 0;

    if(m_stationCount == count) {
        return;
    }
    m_stationCount = count;
    emit stationCountChanged();
}

void cwNodePageModel::updateHiddenNames()
{
    QStringList names;
    const cwCavingRegion* region = m_node != nullptr ? m_node->parentRegion() : nullptr;
    if(region != nullptr && m_node->isSourceRoot()) {
        const cwExternalSourceModel* sources = region->externalSources();
        const int row = sources->indexOf(m_node->sourceId());
        if(row >= 0) {
            const QStringList paths = sources->sourceAt(row).excludedSourcePaths();
            names.reserve(paths.size());
            for(const QString& path : paths) {
                names.append(hiddenName(path, m_node->name()));
            }
        }
    }

    if(m_hiddenNames == names) {
        return;
    }
    m_hiddenNames = names;
    emit hiddenNamesChanged();
}
