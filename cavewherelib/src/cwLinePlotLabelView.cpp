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
            disconnect(Region, nullptr, this, nullptr);
            Region->rootNode()->walk([this](const cwSurveyNode* node) {
                disconnectNode(node);
            });
        }

        Region = region;
        clear();

        if(Region != nullptr) {
            // The region's pulse reaches every node; listening to it once
            // rebuilds every trip's labels in one pass.
            connect(Region, &cwCavingRegion::positionsChanged, this, &cwLinePlotLabelView::updateStations);
            connect(Region, &cwCavingRegion::memberViewsChanged, this, &cwLinePlotLabelView::updateStations);
            addNodes({Region->rootNode()});
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
    updateKeywordItems(m_groups.keys());

    emit keywordItemModelChanged();
}

/**
 * @brief cwLinePlotLabelView::addNodes
 * @param nodes - Nodes that just joined the region, or the region's root
 *
 * Connects each node and every node below it, and adds a label group for each
 * of their trips, registering the trips' keyword items in one insert.
 */
void cwLinePlotLabelView::addNodes(const QList<const cwSurveyNode*>& nodes) {
    QList<cwTrip*> trips;
    for(const cwSurveyNode* node : nodes) {
        node->walk([this, &trips](const cwSurveyNode* subtreeNode) {
            connectNode(subtreeNode);
            trips.append(subtreeNode->trips());
        });
    }
    addTrips(trips);
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
    QList<const cwSurveyNode*> nodes;
    for(int i = begin; i <= end; i++) {
        nodes.append(parentNode->childNode(i));
    }
    addNodes(nodes);
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
    QList<cwTrip*> trips;
    for(int i = begin; i <= end; i++) {
        trips.append(node->trip(i));
    }
    addTrips(trips);
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
 * The region's station positions or member views changed; rebuild the labels
 * of every trip in the region.
 */
void cwLinePlotLabelView::updateStations() {
    const QList<cwTrip*> trips = m_groups.keys();
    for(cwTrip* trip : trips) {
        m_groups.value(trip)->setLabels(labels(trip));
    }
    updateKeywordItems(trips);
}

/**
 * @brief cwLinePlotLabelView::addTrips
 * @param trips - The trips to add a label group for
 */
void cwLinePlotLabelView::addTrips(const QList<cwTrip*>& trips) {
    QList<cwTrip*> added;
    added.reserve(trips.size());
    for(cwTrip* trip : trips) {
        if(m_groups.contains(trip)) {
            continue;
        }

        cwLabel3dGroup* group = new cwLabel3dGroup(this);
        group->setLabels(labels(trip));
        m_groups.insert(trip, group);
        added.append(trip);
    }
    updateKeywordItems(added);
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
 * @brief cwLinePlotLabelView::updateKeywordItems
 *
 * Registers/unregisters each trip's label keyword item so the filter only
 * carries a row for trips that actually have labels: create it when the count
 * crosses 0->1, drop it when it returns to 0. The new items land in the model
 * as one insert. Each item references the trip-owned
 * cwTrip::linePlotKeywordModel() (Type="Line Plot" plus the trip's inherited
 * keywords), the same model the line plot geometry uses, and targets the trip's
 * label group directly (a QObject with a setVisible(bool) slot, dispatched by
 * cwKeywordVisibility), so filtering the line plot hides the labels with it.
 */
void cwLinePlotLabelView::updateKeywordItems(const QList<cwTrip*>& trips) {
    QList<cwTrip*> labeled;
    labeled.reserve(trips.size());
    for(cwTrip* trip : trips) {
        const cwLabel3dGroup* group = m_groups.value(trip, nullptr);
        if(group != nullptr && !group->labels().isEmpty()) {
            labeled.append(trip);
        } else {
            m_keywordRegistry.drop(trip);
        }
    }

    m_keywordRegistry.ensureAll(labeled, [this](cwTrip* trip) {
        auto item = new cwKeywordItem();
        item->keywordModel()->addExtension(trip->linePlotKeywordModel());
        item->setObject(m_groups.value(trip));
        return item;
    });
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
