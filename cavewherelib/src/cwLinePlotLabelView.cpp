/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLinePlotLabelView.h"
#include "cwCavingRegion.h"
#include "cwSurveyNode.h"
#include "cwTrip.h"
#include "cwStation.h"
#include "cwStationPositionLookup.h"
#include "cwLabel3dGroup.h"
#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordModel.h"

//Qt includes
#include <QMap>

cwLinePlotLabelView::cwLinePlotLabelView(QQuickItem *parent) :
    cwLabel3dView(parent),
    Region(nullptr)
{

}

/**
Sets region
*/
void cwLinePlotLabelView::setRegion(cwCavingRegion* region) {
    if(Region != region) {
        if(Region != nullptr) {
            Region->rootNode()->walk([this](const cwSurveyNode* node) {
                disconnectNode(node);
            });
        }

        Region = region;
        clear();

        if(Region != nullptr) {
            addNode(Region->rootNode());
        }
        emit regionChanged();
    }
}

/**
 * @brief cwLinePlotLabelView::keywordItemModel
 * @return The keyword model this view publishes its per-trip label items into.
 */
void cwLinePlotLabelView::setKeywordItemModel(cwKeywordItemModel* keywordItemModel) {
    if(m_keywordRegistry.model() == keywordItemModel) {
        return;
    }

    // Tears down items in the old model; re-register every trip that still has
    // labels against the new one.
    m_keywordRegistry.setModel(keywordItemModel);

    for(auto it = m_groups.keyBegin(); it != m_groups.keyEnd(); ++it) {
        updateKeywordItem(*it);
    }

    emit keywordItemModelChanged();
}

/**
 * @brief cwLinePlotLabelView::addNode
 * @param node - A node that just joined the region, or the region's root
 *
 * Connects the node and every node below it, and adds a label group for each of
 * their trips.
 */
void cwLinePlotLabelView::addNode(const cwSurveyNode* node) {
    node->walk([this](const cwSurveyNode* subtreeNode) {
        connectNode(subtreeNode);
        const QList<cwTrip*> trips = subtreeNode->trips();
        for(cwTrip* trip : trips) {
            addTrip(trip);
        }
    });
}

/**
 * @brief cwLinePlotLabelView::removeNode
 * @param node - A node about to leave the region
 *
 * Removes the label groups for every trip at or below the node.
 */
void cwLinePlotLabelView::removeNode(const cwSurveyNode* node)
{
    node->walk([this](const cwSurveyNode* subtreeNode) {
        disconnectNode(subtreeNode);
        const QList<cwTrip*> trips = subtreeNode->trips();
        for(cwTrip* trip : trips) {
            removeTrip(trip);
        }
    });
}

/**
 * @brief cwLinePlotLabelView::nodesInserted
 *
 * A node gained child nodes; label every trip in each new subtree.
 */
void cwLinePlotLabelView::nodesInserted(int begin, int end) {
    const cwSurveyNode* parentNode = static_cast<cwSurveyNode*>(sender());
    for(int i = begin; i <= end; i++) {
        addNode(parentNode->childNode(i));
    }
}

/**
 * @brief cwLinePlotLabelView::nodesRemoved
 *
 * A node is about to lose child nodes; tear down the label groups of each
 * subtree while its trips are still valid.
 */
void cwLinePlotLabelView::nodesRemoved(int begin, int end) {
    const cwSurveyNode* parentNode = static_cast<cwSurveyNode*>(sender());
    for(int i = end; i >= begin; i--) {
        removeNode(parentNode->childNode(i));
    }
}

/**
 * @brief cwLinePlotLabelView::tripsInserted
 *
 * A node gained trips; add a label group for each new trip.
 */
void cwLinePlotLabelView::tripsInserted(int begin, int end) {
    const cwSurveyNode* node = static_cast<cwSurveyNode*>(sender());
    for(int i = begin; i <= end; i++) {
        addTrip(node->trip(i));
    }
}

/**
 * @brief cwLinePlotLabelView::tripsRemoved
 *
 * A node is about to lose trips; tear down their label groups while the trips
 * are still valid.
 */
void cwLinePlotLabelView::tripsRemoved(int begin, int end) {
    const cwSurveyNode* node = static_cast<cwSurveyNode*>(sender());
    for(int i = begin; i <= end; i++) {
        removeTrip(node->trip(i));
    }
}

/**
 * @brief cwLinePlotLabelView::updateStations
 *
 * A node's station positions changed; rebuild the labels for each of its own
 * trips. Every node gets its own positions, so a child node's trips are rebuilt
 * when the child's signal fires.
 */
void cwLinePlotLabelView::updateStations() {
    const cwSurveyNode* node = static_cast<cwSurveyNode*>(sender());
    const QList<cwTrip*> trips = node->trips();
    for(cwTrip* trip : trips) {
        auto it = m_groups.find(trip);
        if(it != m_groups.end()) {
            it.value()->setLabels(labels(trip));
            updateKeywordItem(trip);
        }
    }
}

/**
 * @brief cwLinePlotLabelView::addTrip
 * @param trip - The trip to add a label group for
 */
void cwLinePlotLabelView::addTrip(cwTrip* trip) {
    if(m_groups.contains(trip)) {
        return;
    }

    cwLabel3dGroup* group = new cwLabel3dGroup(this);
    group->setLabels(labels(trip));

    m_groups.insert(trip, group);
    updateKeywordItem(trip);
}

/**
 * @brief cwLinePlotLabelView::removeTrip
 * @param trip - The trip whose label group is removed
 */
void cwLinePlotLabelView::removeTrip(cwTrip* trip) {
    auto it = m_groups.find(trip);
    if(it == m_groups.end()) {
        return;
    }

    // Drop the keyword item before destroying its target group: setObject() wires
    // the group's destroyed() to self-delete the item, so the registry must let go
    // of it first.
    m_keywordRegistry.drop(trip);
    if(it.value() != nullptr) {
        it.value()->deleteLater();
    }
    m_groups.erase(it);
}

/**
 * @brief cwLinePlotLabelView::connectNode
 * @param node
 */
void cwLinePlotLabelView::connectNode(const cwSurveyNode *node) {
    connect(node, &cwSurveyNode::stationPositionPositionChanged, this, &cwLinePlotLabelView::updateStations);
    connect(node, &cwSurveyNode::insertedTrips, this, &cwLinePlotLabelView::tripsInserted);
    connect(node, &cwSurveyNode::beginRemoveTrips, this, &cwLinePlotLabelView::tripsRemoved);
    connect(node, &cwSurveyNode::insertedNodes, this, &cwLinePlotLabelView::nodesInserted);
    connect(node, &cwSurveyNode::beginRemoveNodes, this, &cwLinePlotLabelView::nodesRemoved);
}

/**
 * @brief cwLinePlotLabelView::disconnectNode
 * @param node
 */
void cwLinePlotLabelView::disconnectNode(const cwSurveyNode *node)
{
    disconnect(node, nullptr, this, nullptr);
}

/**
 * @brief cwLinePlotLabelView::labels
 * @param trip - Supplies its own solved stations (native or externally attached)
 * @return The labels for the trip's stations that have a solved position
 *
 * A station shared by several trips gets a (duplicate) label in each owning
 * trip's group; the view's shared declutter KD-tree rejects the co-located
 * duplicate, so a shared station shows as long as any owning trip is visible.
 */
QList<cwLabel3dItem> cwLinePlotLabelView::labels(cwTrip *trip) const
{
    const QList<QPair<QString, QVector3D>> stations = trip->solvedStations();

    QList<cwLabel3dItem> labels;
    labels.reserve(stations.size());
    for(const auto& station : stations) {
        labels.append(cwLabel3dItem(station.first, station.second));
    }

    return labels;
}

/**
 * @brief cwLinePlotLabelView::updateKeywordItem
 *
 * Registers/unregisters the trip's label keyword item so the filter only carries
 * a row for trips that actually have labels: create it when the count crosses
 * 0->1, drop it when it returns to 0. The item references the trip-owned
 * cwTrip::linePlotKeywordModel() (Type="Line Plot" plus the trip's inherited
 * keywords), the same model the line plot geometry uses, and targets the trip's
 * label group directly (a QObject with a setVisible(bool) slot, dispatched by
 * cwKeywordVisibility), so filtering the line plot hides the labels with it.
 */
void cwLinePlotLabelView::updateKeywordItem(cwTrip* trip) {
    auto it = m_groups.find(trip);
    if(it == m_groups.end()) {
        return;
    }

    cwLabel3dGroup* group = it.value();
    if(group == nullptr || group->labels().isEmpty()) {
        m_keywordRegistry.drop(trip);
    } else {
        m_keywordRegistry.ensure(trip, [trip, group]() {
            auto item = new cwKeywordItem();
            item->keywordModel()->addExtension(trip->linePlotKeywordModel());
            item->setObject(group);
            return item;
        });
    }
}

/**
 * @brief cwLinePlotLabelView::clear
 *
 * Removes all trip label groups and their keyword items from the view.
 */
void cwLinePlotLabelView::clear() {
    for(auto it = m_groups.begin(); it != m_groups.end(); ++it) {
        m_keywordRegistry.drop(it.key());
        if(it.value() != nullptr) {
            it.value()->deleteLater();
        }
    }
    m_groups.clear();
}
