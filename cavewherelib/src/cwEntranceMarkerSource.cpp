/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwEntranceMarkerSource.h"
#include "cwEntranceLabel.h"
#include "cwEntranceModel.h"
#include "cwKeywordModel.h"
#include "cwLength.h"
#include "cwStation.h"
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

// The keyword type every entrance marker carries.
const QString kEntranceType = QStringLiteral("Entrance");

double toMeters(const cwLength* length)
{
    return cwUnits::convert(length->value(),
                            static_cast<cwUnits::LengthUnit>(length->unit()),
                            cwUnits::Meters);
}

// One marker per station of one node, stable across solves and label edits.
QUuid entranceId(const cwSurveyNode* node, const QString& stationName)
{
    return QUuid::createUuidV5(node->id(), cwStation::canonicalKey(stationName.trimmed()));
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

int cwEntranceMarkerSource::row(cwSurveyNode* node, const QString& stationName) const
{
    if(node == nullptr) {
        return -1;
    }
    const QUuid id = entranceId(node, stationName);
    for(int i = 0; i < m_entrances.size(); ++i) {
        if(m_entrances.at(i).id == id) {
            return i;
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

    // The root relays subtreeChanged for every node below it, so one
    // connection covers node inserts and removals.
    connect(m_region->rootNode(), &cwSurveyNode::subtreeChanged, this, &cwEntranceMarkerSource::scheduleRebuild);
    connect(m_region, &cwCavingRegion::solvedEntrancesChanged, this, &cwEntranceMarkerSource::scheduleRebuild);
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
        connect(node, &cwSurveyNode::kindChanged, this, &cwEntranceMarkerSource::scheduleRebuild);
        connect(node->length(), &cwLength::valueChanged, this, &cwEntranceMarkerSource::scheduleRebuild);
        connect(node->length(), &cwLength::unitChanged, this, &cwEntranceMarkerSource::scheduleRebuild);
        connect(node->depth(), &cwLength::valueChanged, this, &cwEntranceMarkerSource::scheduleRebuild);
        connect(node->depth(), &cwLength::unitChanged, this, &cwEntranceMarkerSource::scheduleRebuild);

        cwEntranceModel* entrances = node->entrances();
        connect(entrances, &QAbstractItemModel::rowsInserted, this, &cwEntranceMarkerSource::scheduleRebuild);
        connect(entrances, &QAbstractItemModel::rowsRemoved, this, &cwEntranceMarkerSource::scheduleRebuild);
        connect(entrances, &QAbstractItemModel::dataChanged, this, &cwEntranceMarkerSource::scheduleRebuild);
        connect(entrances, &QAbstractItemModel::modelReset, this, &cwEntranceMarkerSource::scheduleRebuild);
    }
}

void cwEntranceMarkerSource::disconnectNode(cwSurveyNode* node)
{
    disconnect(node, nullptr, this, nullptr);
    disconnect(node->length(), nullptr, this, nullptr);
    disconnect(node->depth(), nullptr, this, nullptr);
    disconnect(node->entrances(), nullptr, this, nullptr);
}

void cwEntranceMarkerSource::nodeDestroyed(QObject* node)
{
    // Only the address is used; the node is mid-destruction.
    m_connectedNodes.remove(static_cast<cwSurveyNode*>(node));
    scheduleRebuild();
}

/**
 * The entrances to draw: every node's solved entrances, named by the owning
 * node's entrance label or the name of the node they sit in, with that node's
 * numbers. An entrance whose deepest node is its owning node sits in that
 * node's cave.
 */
QList<cwEntranceMarkerSource::Entrance> cwEntranceMarkerSource::desiredEntrances() const
{
    QList<Entrance> desired;
    if(m_region.isNull()) {
        return desired;
    }

    const QHash<QUuid, QList<cwSolvedEntrance>> solved = m_region->solvedEntrances();
    if(solved.isEmpty()) {
        return desired;
    }

    const cwUnits::UnitSystem unitSystem = m_region->unitSystem();
    const QList<cwSurveyNode*> nodes = m_region->rootNode()->allNodes();
    QHash<QUuid, cwSurveyNode*> nodesById;
    nodesById.reserve(nodes.size());
    for(cwSurveyNode* node : nodes) {
        nodesById.insert(node->id(), node);
    }

    QHash<const cwSurveyNode*, QString> statsTextByNode;
    const auto statsTextOf = [&](cwSurveyNode* node) {
        auto it = statsTextByNode.constFind(node);
        if(it == statsTextByNode.constEnd()) {
            it = statsTextByNode.insert(node, cwEntranceLabel::statsText(toMeters(node->length()),
                                                                         toMeters(node->depth()),
                                                                         unitSystem));
        }
        return it.value();
    };

    for(cwSurveyNode* node : nodes) {
        const auto nodeEntrances = solved.constFind(node->id());
        if(nodeEntrances == solved.constEnd()) {
            continue;
        }

        const cwEntranceModel* entranceModel = node->entrances();
        for(const cwSolvedEntrance& solvedEntrance : nodeEntrances.value()) {
            // Cavern folds native names to lower case; a native entrance keeps
            // the spelling of its entrance entry.
            const int entryRow = entranceModel->indexOf(solvedEntrance.stationName);
            const cwEntrance entry = entranceModel->entranceAt(entryRow);

            cwSurveyNode* deepestNode = nodesById.value(solvedEntrance.deepestNodeId, node);
            cwSurveyNode* caveNode = deepestNode == node ? node->caveNode() : deepestNode;

            Entrance entrance;
            entrance.id = entranceId(node, solvedEntrance.stationName);
            entrance.node = node;
            entrance.caveNode = caveNode;
            entrance.deepestNode = deepestNode;
            entrance.stationName = entryRow >= 0 ? entry.stationName() : solvedEntrance.stationName;
            entrance.position = solvedEntrance.position;
            entrance.fixed = solvedEntrance.fixed;
            entrance.caveName = caveNode->name();
            entrance.label = entry.label();
            entrance.statsText = statsTextOf(caveNode);
            desired.append(entrance);
        }
    }
    return desired;
}

/**
 * The marker of one entrance: the pin at its solved position, the display name
 * over the stats line, and a keyword item of its own that extends its node's
 * keywords with the Entrance type and its name.
 */
cwMarker cwEntranceMarkerSource::toMarker(const Entrance& entrance)
{
    const QString displayName = entrance.label.isEmpty() ? entrance.caveName : entrance.label;

    cwMarker marker;
    marker.id = entrance.id;
    marker.position = entrance.position;
    marker.textLines = {{displayName, kNameStyle},
                        {entrance.statsText, kStatsStyle}};
    marker.visibilityGroup = entrance.id.toString(QUuid::WithoutBraces);
    marker.keywordExtension = entrance.node != nullptr ? entrance.node->keywordModel() : nullptr;
    marker.keywords = {cwKeyword(cwKeywordModel::TypeKey, kEntranceType),
                       cwKeyword(cwKeywordModel::NameKey, displayName)};
    marker.delegateProperties = {{QStringLiteral("node"), QVariant::fromValue(entrance.node)},
                                 {QStringLiteral("caveNode"), QVariant::fromValue(entrance.caveNode)},
                                 {QStringLiteral("deepestNode"), QVariant::fromValue(entrance.deepestNode)},
                                 {QStringLiteral("stationName"), entrance.stationName},
                                 {QStringLiteral("caveName"), entrance.caveName},
                                 {QStringLiteral("label"), entrance.label},
                                 {QStringLiteral("fixed"), entrance.fixed},
                                 {QStringLiteral("displayName"), displayName}};
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
 * The one pass every solve, entrance list, name, size and unit change funnels into. It
 * diffs the drawn entrances against desiredEntrances() and announces the
 * difference with at most one signal of each kind per contiguous range: removed
 * rows first, then the changed rows in place, then the new ones appended.
 */
void cwEntranceMarkerSource::rebuild()
{
    m_rebuildPending = false;
    syncNodeConnections();

    // A station a solve reports twice for one node draws once, with its last occurrence.
    QList<Entrance> desired;
    QHash<QUuid, int> desiredRows;
    const QList<Entrance> found = desiredEntrances();
    for(const Entrance& entrance : found) {
        const auto it = desiredRows.constFind(entrance.id);
        if(it == desiredRows.constEnd()) {
            desiredRows.insert(entrance.id, int(desired.size()));
            desired.append(entrance);
        } else {
            desired[it.value()] = entrance;
        }
    }

    // Removed rows, as contiguous ranges from the end so earlier rows keep their index.
    for(int last = int(m_entrances.size()) - 1; last >= 0;) {
        if(desiredRows.contains(m_entrances.at(last).id)) {
            --last;
            continue;
        }
        int first = last;
        while(first > 0 && !desiredRows.contains(m_entrances.at(first - 1).id)) {
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
        const Entrance& want = desired.at(desiredRows.value(m_entrances.at(row).id));
        kept.insert(want.id);
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
        if(!kept.contains(entrance.id)) {
            added.append(entrance);
        }
    }
    if(!added.isEmpty()) {
        const int first = int(m_entrances.size());
        m_entrances.append(added);
        insertMarkers(first, toMarkers(added));
    }
}
