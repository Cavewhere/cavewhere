/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLinePlotLabelView.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwStation.h"
#include "cwStationPositionLookup.h"
#include "cwLabel3dGroup.h"
#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordModel.h"

cwLinePlotLabelView::cwLinePlotLabelView(QQuickItem *parent) :
    cwLabel3dView(parent),
    Region(nullptr)
{

}

cwLinePlotLabelView::~cwLinePlotLabelView()
{
    // The keyword items are owned by the model (addItem reparents them), so they
    // outlive the view unless removed here. Synchronous delete: the event loop
    // won't run again to drain deleteLater during teardown.
    for(auto it = m_tripEntries.begin(); it != m_tripEntries.end(); ++it) {
        cwKeywordItem* item = it.value().keywordItem;
        if(item != nullptr) {
            if(!m_keywordItemModel.isNull()) {
                m_keywordItemModel->removeItem(item);
            }
            delete item;
            it.value().keywordItem = nullptr;
        }
    }
}

/**
Sets region
*/
void cwLinePlotLabelView::setRegion(cwCavingRegion* region) {
    if(Region != region) {
        if(Region != nullptr) {
            disconnect(Region, &cwCavingRegion::insertedCaves, this, &cwLinePlotLabelView::addCaves);
            disconnect(Region, &cwCavingRegion::beginRemoveCaves, this, &cwLinePlotLabelView::removeCaves);

            for(cwCave* cave : Region->caves()) {
                disconnectCave(cave);
            }
        }

        Region = region;
        clear();

        if(Region != nullptr) {
            connect(Region, &cwCavingRegion::insertedCaves, this, &cwLinePlotLabelView::addCaves);
            connect(Region, &cwCavingRegion::beginRemoveCaves, this, &cwLinePlotLabelView::removeCaves);

            if(Region->hasCaves()) {
                //Add all the caves
                addCaves(0, Region->caveCount() - 1);
            }
        }
        emit regionChanged();
    }
}

/**
 * @brief cwLinePlotLabelView::keywordItemModel
 * @return The keyword model this view publishes its per-trip label items into.
 */
void cwLinePlotLabelView::setKeywordItemModel(cwKeywordItemModel* keywordItemModel) {
    if(m_keywordItemModel == keywordItemModel) {
        return;
    }

    // Remove existing items from the old model before switching.
    for(auto it = m_tripEntries.begin(); it != m_tripEntries.end(); ++it) {
        removeKeywordItem(it.value());
    }

    m_keywordItemModel = keywordItemModel;

    // Re-register every trip that currently has labels against the new model.
    for(auto it = m_tripEntries.keyBegin(); it != m_tripEntries.keyEnd(); ++it) {
        updateKeywordItem(*it);
    }

    emit keywordItemModelChanged();
}

/**
 * @brief cwLinePlotLabelView::addCaves
 * @param begin - The index of the first cave that's added to the Region
 * @param end - The index of the last cave that's added to the region
 *
 * Connects each new cave and adds a label group for each of its trips.
 */
void cwLinePlotLabelView::addCaves(int begin, int end) {
    for(int i = begin; i <= end; i++) {
        cwCave* cave = Region->cave(i);
        connectCave(cave);

        for(cwTrip* trip : cave->trips()) {
            addTrip(cave, trip);
        }
    }
}

/**
 * @brief cwLinePlotLabelView::removeCaves
 * @param begin
 * @param end
 *
 * Removes the label groups for every trip in the caves between begin and end.
 */
void cwLinePlotLabelView::removeCaves(int begin, int end)
{
    for(int i = end; i >= begin; i--) {
        cwCave* cave = Region->cave(i);
        disconnectCave(cave);

        for(cwTrip* trip : cave->trips()) {
            removeTrip(trip);
        }
    }
}

/**
 * @brief cwLinePlotLabelView::caveTripsInserted
 *
 * A cave gained trips; add a label group for each new trip.
 */
void cwLinePlotLabelView::caveTripsInserted(int begin, int end) {
    cwCave* cave = static_cast<cwCave*>(sender());
    for(int i = begin; i <= end; i++) {
        addTrip(cave, cave->trip(i));
    }
}

/**
 * @brief cwLinePlotLabelView::caveTripsRemoved
 *
 * A cave is about to lose trips; tear down their label groups while the trips
 * are still valid.
 */
void cwLinePlotLabelView::caveTripsRemoved(int begin, int end) {
    cwCave* cave = static_cast<cwCave*>(sender());
    for(int i = begin; i <= end; i++) {
        removeTrip(cave->trip(i));
    }
}

/**
 * @brief cwLinePlotLabelView::updateStations
 *
 * A cave's station positions changed; rebuild the labels for every trip in it.
 */
void cwLinePlotLabelView::updateStations() {
    cwCave* cave = static_cast<cwCave*>(sender());
    for(cwTrip* trip : cave->trips()) {
        auto it = m_tripEntries.find(trip);
        if(it != m_tripEntries.end()) {
            it.value().group->setLabels(labels(cave, trip));
            updateKeywordItem(trip);
        }
    }
}

/**
 * @brief cwLinePlotLabelView::addTrip
 * @param cave - The cave that owns the trip (its station position lookup positions the labels)
 * @param trip - The trip to add a label group for
 */
void cwLinePlotLabelView::addTrip(cwCave* cave, cwTrip* trip) {
    if(m_tripEntries.contains(trip)) {
        return;
    }

    cwLabel3dGroup* group = new cwLabel3dGroup(this);
    group->setLabels(labels(cave, trip));

    m_tripEntries.insert(trip, {group, nullptr});
    updateKeywordItem(trip);
}

/**
 * @brief cwLinePlotLabelView::removeTrip
 * @param trip - The trip whose label group is removed
 */
void cwLinePlotLabelView::removeTrip(cwTrip* trip) {
    auto it = m_tripEntries.find(trip);
    if(it == m_tripEntries.end()) {
        return;
    }

    removeKeywordItem(it.value());
    if(it.value().group != nullptr) {
        it.value().group->deleteLater();
    }
    m_tripEntries.erase(it);
}

/**
 * @brief cwLinePlotLabelView::connectCave
 * @param cave
 */
void cwLinePlotLabelView::connectCave(cwCave *cave) {
    connect(cave, &cwCave::stationPositionPositionChanged, this, &cwLinePlotLabelView::updateStations);
    connect(cave, &cwCave::insertedTrips, this, &cwLinePlotLabelView::caveTripsInserted);
    connect(cave, &cwCave::beginRemoveTrips, this, &cwLinePlotLabelView::caveTripsRemoved);
}

/**
 * @brief cwLinePlotLabelView::disconnectCave
 * @param cave
 */
void cwLinePlotLabelView::disconnectCave(cwCave *cave)
{
    disconnect(cave, &cwCave::stationPositionPositionChanged, this, &cwLinePlotLabelView::updateStations);
    disconnect(cave, &cwCave::insertedTrips, this, &cwLinePlotLabelView::caveTripsInserted);
    disconnect(cave, &cwCave::beginRemoveTrips, this, &cwLinePlotLabelView::caveTripsRemoved);
}

/**
 * @brief cwLinePlotLabelView::labels
 * @param cave - Supplies the solved station positions
 * @param trip - Supplies the trip's unique stations
 * @return The labels for the trip's stations that have a solved position
 *
 * A station shared by several trips gets a (duplicate) label in each owning
 * trip's group; the view's shared declutter KD-tree rejects the co-located
 * duplicate, so a shared station shows as long as any owning trip is visible.
 */
QList<cwLabel3dItem> cwLinePlotLabelView::labels(cwCave *cave, cwTrip *trip) const
{
    const cwStationPositionLookup positions = cave->stationPositionLookup();

    QList<cwLabel3dItem> labels;
    const QList<cwStation> stations = trip->uniqueStations();
    labels.reserve(stations.size());

    for(const cwStation& station : stations) {
        if(positions.hasPosition(station.name())) {
            labels.append(cwLabel3dItem(station.name(), positions.position(station.name())));
        }
    }

    return labels;
}

/**
 * @brief cwLinePlotLabelView::updateKeywordItem
 *
 * Registers/unregisters the trip's label keyword item so the filter only carries
 * a row for trips that actually have labels: create it when the count crosses
 * 0->1, remove it when it returns to 0.
 */
void cwLinePlotLabelView::updateKeywordItem(cwTrip* trip) {
    auto it = m_tripEntries.find(trip);
    if(it == m_tripEntries.end()) {
        return;
    }

    TripEntry& entry = it.value();
    if(entry.group == nullptr || entry.group->labels().isEmpty()) {
        removeKeywordItem(entry);
    } else {
        addKeywordItem(trip, entry.group);
    }
}

/**
 * @brief cwLinePlotLabelView::addKeywordItem
 *
 * Publishes one cwKeywordItem for the trip, referencing the trip-owned
 * cwTrip::linePlotKeywordModel() (Type="Line Plot" plus the trip's inherited
 * keywords), the same model the line plot geometry uses. The trip's label group
 * is the setVisible() target, so filtering the line plot hides the labels with
 * it. Mirrors cwLeadView / cwLinePlotManager.
 */
void cwLinePlotLabelView::addKeywordItem(cwTrip* trip, cwLabel3dGroup* group) {
    if(m_keywordItemModel.isNull()) {
        return;
    }

    auto it = m_tripEntries.find(trip);
    if(it == m_tripEntries.end() || it.value().keywordItem != nullptr) {
        return;
    }

    auto item = new cwKeywordItem();
    item->keywordModel()->addExtension(trip->linePlotKeywordModel());

    // The group is a QObject with a setVisible(bool) slot, so it is the keyword
    // target directly (no proxy) via cwKeywordVisibility's generic dispatch.
    item->setObject(group);

    it.value().keywordItem = item;
    m_keywordItemModel->addItem(item);
}

void cwLinePlotLabelView::removeKeywordItem(TripEntry& entry) {
    if(entry.keywordItem == nullptr) {
        return;
    }

    if(!m_keywordItemModel.isNull()) {
        m_keywordItemModel->removeItem(entry.keywordItem);
    }
    entry.keywordItem->deleteLater();
    entry.keywordItem = nullptr;
}

/**
 * @brief cwLinePlotLabelView::clear
 *
 * Removes all trip label groups and their keyword items from the view.
 */
void cwLinePlotLabelView::clear() {
    for(auto it = m_tripEntries.begin(); it != m_tripEntries.end(); ++it) {
        removeKeywordItem(it.value());
        if(it.value().group != nullptr) {
            it.value().group->deleteLater();
        }
    }
    m_tripEntries.clear();
}
