/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwEntranceMarkerSource.h"
#include "cwEntranceLabel.h"
#include "cwFixStationModel.h"
#include "cwKeywordModel.h"
#include "cwLength.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyNode.h"

//Qt includes
#include <QHash>

namespace {
// The pin's tip is the bottom center of its sprite, so the tip sits on the station.
constexpr QPointF kPinAnchor(0.5, 1.0);

// Pulls pin and text toward the viewer, further than leads, so the marker at a
// surface station clears the terrain around it.
constexpr float kEntranceDepthBias = 2.0f;

// The text styles a MarkerView declares for the entrance lines.
const QString kNameStyle = QStringLiteral("name");
const QString kStatsStyle = QStringLiteral("stats");

double toMeters(const cwLength* length)
{
    return cwUnits::convert(length->value(),
                            static_cast<cwUnits::LengthUnit>(length->unit()),
                            cwUnits::Meters);
}
}

cwEntranceMarkerSource::cwEntranceMarkerSource(QObject* parent) :
    cwMarkerSource(parent)
{
}

cwCavingRegion* cwEntranceMarkerSource::region() const
{
    return m_region;
}

void cwEntranceMarkerSource::setRegion(cwCavingRegion* region)
{
    if(m_region == region) {
        return;
    }
    disconnectRegion();
    m_region = region;
    connectRegion();
    scheduleRebuild();
    emit regionChanged();
}

int cwEntranceMarkerSource::row(cwSurveyNode* node, const QUuid& fixId) const
{
    for(int i = 0; i < m_entrances.size(); ++i) {
        const Entrance& entrance = m_entrances.at(i);
        if(entrance.fixId == fixId) {
            return entrance.node == node ? i : -1;
        }
    }
    return -1;
}

QPointF cwEntranceMarkerSource::spriteAnchor() const
{
    return kPinAnchor;
}

float cwEntranceMarkerSource::depthBias() const
{
    return kEntranceDepthBias;
}

void cwEntranceMarkerSource::scheduleRebuild()
{
    if(m_rebuildPending) {
        return;
    }
    m_rebuildPending = true;
    QMetaObject::invokeMethod(this, &cwEntranceMarkerSource::rebuild, Qt::QueuedConnection);
}

void cwEntranceMarkerSource::connectRegion()
{
    if(m_region.isNull()) {
        return;
    }

    // The root relays both signals for every node below it, so one connection
    // each covers node inserts, removals and every node's solve.
    cwSurveyNode* root = m_region->rootNode();
    connect(root, &cwSurveyNode::subtreeChanged, this, &cwEntranceMarkerSource::scheduleRebuild);
    connect(root, &cwSurveyNode::scopedPositionsChanged, this, &cwEntranceMarkerSource::scheduleRebuild);
    connect(m_region, &cwCavingRegion::unitSystemChanged, this, &cwEntranceMarkerSource::scheduleRebuild);
    connect(m_region, &QObject::destroyed, this, &cwEntranceMarkerSource::scheduleRebuild);
}

void cwEntranceMarkerSource::disconnectRegion()
{
    if(!m_region.isNull()) {
        disconnect(m_region->rootNode(), nullptr, this, nullptr);
        disconnect(m_region, nullptr, this, nullptr);
    }

    for(cwSurveyNode* node : std::as_const(m_connectedNodes)) {
        disconnectNode(node);
    }
    m_connectedNodes.clear();
}

void cwEntranceMarkerSource::syncNodeConnections()
{
    const QList<cwSurveyNode*> current = m_region.isNull()
                                             ? QList<cwSurveyNode*>()
                                             : m_region->rootNode()->allNodes();
    const QSet<cwSurveyNode*> currentSet(current.begin(), current.end());

    // Every node still in m_connectedNodes is alive: nodeDestroyed drops the others.
    for(auto it = m_connectedNodes.begin(); it != m_connectedNodes.end();) {
        if(currentSet.contains(*it)) {
            ++it;
        } else {
            disconnectNode(*it);
            it = m_connectedNodes.erase(it);
        }
    }

    for(cwSurveyNode* node : current) {
        if(m_connectedNodes.contains(node)) {
            continue;
        }
        m_connectedNodes.insert(node);

        connect(node, &QObject::destroyed, this, &cwEntranceMarkerSource::nodeDestroyed);
        connect(node, &cwSurveyNode::nameChanged, this, &cwEntranceMarkerSource::scheduleRebuild);
        connect(node->length(), &cwLength::valueChanged, this, &cwEntranceMarkerSource::scheduleRebuild);
        connect(node->length(), &cwLength::unitChanged, this, &cwEntranceMarkerSource::scheduleRebuild);
        connect(node->depth(), &cwLength::valueChanged, this, &cwEntranceMarkerSource::scheduleRebuild);
        connect(node->depth(), &cwLength::unitChanged, this, &cwEntranceMarkerSource::scheduleRebuild);

        cwFixStationModel* fixes = node->fixStations();
        connect(fixes, &QAbstractItemModel::rowsInserted, this, &cwEntranceMarkerSource::scheduleRebuild);
        connect(fixes, &QAbstractItemModel::rowsRemoved, this, &cwEntranceMarkerSource::scheduleRebuild);
        connect(fixes, &QAbstractItemModel::dataChanged, this, &cwEntranceMarkerSource::scheduleRebuild);
        connect(fixes, &QAbstractItemModel::modelReset, this, &cwEntranceMarkerSource::scheduleRebuild);
    }
}

void cwEntranceMarkerSource::disconnectNode(cwSurveyNode* node)
{
    disconnect(node, nullptr, this, nullptr);
    disconnect(node->length(), nullptr, this, nullptr);
    disconnect(node->depth(), nullptr, this, nullptr);
    disconnect(node->fixStations(), nullptr, this, nullptr);
}

void cwEntranceMarkerSource::nodeDestroyed(QObject* node)
{
    // Only the address is used; the node is mid-destruction.
    m_connectedNodes.remove(static_cast<cwSurveyNode*>(node));
    scheduleRebuild();
}

/**
 * The fixes to draw: a node's own fixes whose coordinate places them and whose
 * station the solve placed below that node.
 */
QList<cwEntranceMarkerSource::Entrance> cwEntranceMarkerSource::desiredEntrances() const
{
    QList<Entrance> desired;
    if(m_region.isNull()) {
        return desired;
    }

    const cwUnits::UnitSystem unitSystem = m_region->unitSystem();
    const QList<cwSurveyNode*> nodes = m_region->rootNode()->allNodes();
    for(cwSurveyNode* node : nodes) {
        const QList<cwFixStation>& fixes = node->fixStations()->fixStations();
        if(fixes.isEmpty()) {
            continue;
        }

        const cwStationPositionLookup positions = node->scopedPositions();
        const double lengthMeters = toMeters(node->length());
        const double depthMeters = toMeters(node->depth());
        for(const cwFixStation& fix : fixes) {
            if(!fix.hasPlacedCoordinate() || !positions.hasPosition(fix.stationName())) {
                continue;
            }

            Entrance entrance;
            entrance.fixId = fix.id();
            entrance.node = node;
            entrance.stationName = fix.stationName();
            entrance.position = positions.position(fix.stationName());
            entrance.kind = fix.kind();
            entrance.displayName = fix.label().isEmpty() ? node->name() : fix.label();
            entrance.statsText = cwEntranceLabel::statsText(entrance.kind, lengthMeters, depthMeters,
                                                            unitSystem);
            desired.append(entrance);
        }
    }
    return desired;
}

/**
 * The marker of one entrance: the pin at its solved position, the display name
 * over the stats line, and a keyword item of its own that extends its node's
 * keywords with its kind and name.
 */
cwMarker cwEntranceMarkerSource::toMarker(const Entrance& entrance)
{
    const QString kindName = cwFixStation::kindName(entrance.kind);
    const QString fixId = entrance.fixId.toString(QUuid::WithoutBraces);

    cwMarker marker;
    marker.id = entrance.fixId;
    marker.position = entrance.position;
    marker.textLines = {{entrance.displayName, kNameStyle},
                        {entrance.statsText, kStatsStyle}};
    marker.visibilityGroup = fixId;
    marker.keywordExtension = entrance.node != nullptr ? entrance.node->keywordModel() : nullptr;
    marker.keywords = {cwKeyword(cwKeywordModel::TypeKey, kindName),
                       cwKeyword(cwKeywordModel::NameKey, entrance.displayName)};
    marker.delegateProperties = {{QStringLiteral("node"), QVariant::fromValue(entrance.node)},
                                 {QStringLiteral("stationName"), entrance.stationName},
                                 {QStringLiteral("fixId"), fixId},
                                 {QStringLiteral("displayName"), entrance.displayName},
                                 {QStringLiteral("kind"), static_cast<int>(entrance.kind)},
                                 {QStringLiteral("kindName"), kindName}};
    return marker;
}

QList<cwMarker> cwEntranceMarkerSource::toMarkers(const QList<Entrance>& entrances)
{
    QList<cwMarker> markers;
    markers.reserve(entrances.size());
    for(const Entrance& entrance : entrances) {
        markers.append(toMarker(entrance));
    }
    return markers;
}

/**
 * The one pass every model, solve, name, size and unit change funnels into. It
 * diffs the drawn entrances against desiredEntrances() and announces the
 * difference with at most one signal of each kind per contiguous range: removed
 * rows first, then the changed rows in place, then the new ones appended.
 */
void cwEntranceMarkerSource::rebuild()
{
    m_rebuildPending = false;
    syncNodeConnections();

    // A fix id repeated across nodes draws once, with its last occurrence.
    QList<Entrance> desired;
    QHash<QUuid, int> desiredRows;
    const QList<Entrance> found = desiredEntrances();
    for(const Entrance& entrance : found) {
        const auto it = desiredRows.constFind(entrance.fixId);
        if(it == desiredRows.constEnd()) {
            desiredRows.insert(entrance.fixId, int(desired.size()));
            desired.append(entrance);
        } else {
            desired[it.value()] = entrance;
        }
    }

    // Removed rows, as contiguous ranges from the end so earlier rows keep their index.
    for(int last = int(m_entrances.size()) - 1; last >= 0;) {
        if(desiredRows.contains(m_entrances.at(last).fixId)) {
            --last;
            continue;
        }
        int first = last;
        while(first > 0 && !desiredRows.contains(m_entrances.at(first - 1).fixId)) {
            --first;
        }
        m_entrances.remove(first, last - first + 1);
        removeMarkers(first, last);
        last = first - 1;
    }

    // Kept rows change in place, in one range from the first change to the last.
    QSet<QUuid> kept;
    kept.reserve(m_entrances.size());
    int firstChanged = -1;
    int lastChanged = -1;
    for(int row = 0; row < m_entrances.size(); ++row) {
        const Entrance& want = desired.at(desiredRows.value(m_entrances.at(row).fixId));
        kept.insert(want.fixId);
        if(!(m_entrances.at(row) == want)) {
            m_entrances[row] = want;
            if(firstChanged < 0) {
                firstChanged = row;
            }
            lastChanged = row;
        }
    }
    if(firstChanged >= 0) {
        replaceMarkers(firstChanged, toMarkers(m_entrances.mid(firstChanged, lastChanged - firstChanged + 1)));
    }

    QList<Entrance> added;
    for(const Entrance& entrance : desired) {
        if(!kept.contains(entrance.fixId)) {
            added.append(entrance);
        }
    }
    if(!added.isEmpty()) {
        const int first = int(m_entrances.size());
        m_entrances.append(added);
        insertMarkers(first, toMarkers(added));
    }
}
