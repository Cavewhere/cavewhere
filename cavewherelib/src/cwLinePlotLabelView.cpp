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
        auto it = m_groups.find(trip);
        if(it != m_groups.end()) {
            it.value()->setLabels(labels(cave, trip));
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
    if(m_groups.contains(trip)) {
        return;
    }

    cwLabel3dGroup* group = new cwLabel3dGroup(this);
    group->setLabels(labels(cave, trip));

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
