//Our includes
#include "cwLinePlotLabelView.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwLabel3dGroup.h"

cwLinePlotLabelView::cwLinePlotLabelView(QQuickItem *parent) :
    cwLabel3dView(parent),
    Region(NULL)
{
}

/**
Sets region
*/
void cwLinePlotLabelView::setRegion(cwCavingRegion* region) {
    if(Region != region) {
        if(Region != NULL) {
            disconnect(Region, &cwCavingRegion::insertedCaves, this, &cwLinePlotLabelView::addCaves);
            disconnect(Region, &cwCavingRegion::beginRemoveCaves, this, &cwLinePlotLabelView::removeCaves);
        }

        Region = region;
        clear();

        if(Region != NULL) {
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
 * @brief cwLinePlotLabelView::addCaves
 * @param begin - The index of the first cave that's added to the Region
 * @param end - The index of the last cave that's added to the region
 *
 * This will add all the labels to the 3d label from the cave
 */
void cwLinePlotLabelView::addCaves(int begin, int end) {
    for(int i = begin; i <= end; i++) {
        cwCave* cave = Region->cave(i);
        connectCave(cave);

        cwLabel3dGroup* labelGroup = new cwLabel3dGroup(this);
        QList<cwLabel3dItem> caveLabels = labels(cave);
        labelGroup->setLabels(caveLabels);

        CaveLabelGroups.insert(i, labelGroup);
    }
}

/**
 * @brief cwLinePlotLabelView::removeCaves
 * @param begin
 * @param end
 *
 * This will remove all the labels from the caves between the begin and end
 */
void cwLinePlotLabelView::removeCaves(int begin, int end)
{
    for(int i = end; i >= begin; i--) {
        cwCave* cave = Region->cave(i);
        disconnectCave(cave);

        cwLabel3dGroup* group = CaveLabelGroups.at(i);
        group->deleteLater();
    }
}

/**
 * @brief cwLinePlotLabelView::updateStations
 */
void cwLinePlotLabelView::updateStations() {
    cwCave* cave = static_cast<cwCave*>(sender());
    updateCaveStations(cave);
}

/**
 * Updates all the sations for the cave
 */
void cwLinePlotLabelView::updateCaveStations(cwCave *cave) {
    QList<cwLabel3dItem> caveLabels = labels(cave);
    int groupIndex = Region->indexOf(cave);
    cwLabel3dGroup* group = CaveLabelGroups.at(groupIndex);
    group->setLabels(caveLabels);
}

/**
 * @brief cwLinePlotLabelView::connectCave
 * @param cave
 */
void cwLinePlotLabelView::connectCave(cwCave *cave) {
    connect(cave, &cwCave::stationPositionModelChanged, this, &cwLinePlotLabelView::updateStations);
}

/**
 * @brief cwLinePlotLabelView::disconnectCave
 * @param cave
 */
void cwLinePlotLabelView::disconnectCave(cwCave *cave)
{
    disconnect(cave, &cwCave::stationPositionModelChanged, this, &cwLinePlotLabelView::updateStations);
}

/**
 * @brief cwLinePlotLabelView::labels
 * @param cave
 * @return
 *
 * Generates labels from the cave
 */
QList<cwLabel3dItem> cwLinePlotLabelView::labels(cwCave *cave) const
{
    cwStationPositionLookup stations = cave->stationPositionModel();

    QList< cwLabel3dItem > uniqueStations;
    uniqueStations.reserve(stations.positions().count());

    QFont font;
    font.setPointSize(10);

    //Populate the vector of unique stations, this is so we can thread the transformation
    QMapIterator<QString, QVector3D> mapIter(stations.positions());
    while(mapIter.hasNext()) {
        mapIter.next();
        uniqueStations.append(cwLabel3dItem(mapIter.key(), mapIter.value(), font));
    }

    return uniqueStations;
}

/**
 * @brief cwLinePlotLabelView::clear
 *
 * Removes all cave labels from the view
 */
void cwLinePlotLabelView::clear() {
    foreach(cwLabel3dGroup* group, CaveLabelGroups) {
        group->deleteLater();
    }
    CaveLabelGroups.clear();
}
