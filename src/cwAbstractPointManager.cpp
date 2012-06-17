#include "cwAbstractPointManager.h"

cwAbstractPointManager::cwAbstractPointManager(QQuickItem *parent) :
    QQuickItem(parent),
    TransformUpdater(NULL),
    TransformNodeDirty(false),
    PointComponent(NULL),
    SelectedIndex(-1)
{

}

/**
  \brief Sets the transform updater

  This will transform all the station in this station view
  */
void cwAbstractPointManager::setTransformUpdater(cwTransformUpdater* updater) {
    if(TransformUpdater != updater) {
        if(TransformUpdater != NULL) {
            //Remove all previous stations
            foreach(QQuickItem* item, PointItems) {
                TransformUpdater->removePointItem(item);
            }
        }

        TransformUpdater = updater;
        TransformNodeDirty = true;

        if(TransformUpdater != NULL) {
            //Add station to the new transformUpdater
            foreach(QQuickItem* item, PointItems) {
                TransformUpdater->addPointItem(item);
            }
        }

        emit transformUpdaterChanged();
    }
}

/**
  This creates the station component used generate the station symobols
  */
void cwAbstractPointManager::createComponent() {
    //Make sure we have a note component so we can create it
    if(PointComponent == NULL) {
        QQmlContext* context = QQmlEngine::contextForObject(this);
        if(context == NULL) { return; }
        PointComponent = new QQmlComponent(context->engine(), qmlSource(), this);
        if(PointComponent->isError()) {
            qDebug() << "Point errors:" << StationItemComponent->errorString();
        }
    }
}

/**
  This is a private method, that adds a new station the end of the station list

    Returns a valid item if the item was create okay, and NULL if it failed to be created
*/
QQuickItem* cwAbstractPointManager::addItem() {
    if(PointComponent == NULL) {
        qDebug() << "PointComponent isn't ready, ... " << qmlSource() << "Didn't compile THIS IS A BUG" << LOCATION;
        return NULL;
    }

    QQmlContext* context = QQmlEngine::contextForObject(this);
    QQuickItem* item = qobject_cast<QQuickItem*>(PointComponent->create(context));
    if(item == NULL) {
        qDebug() << "Problem creating new point item ... " << qmlSource() << "Didn't compile. THIS IS A BUG!" << LOCATION;
        return NULL;
    }

    PointItems.append(item);

    //Add the point to the transform updater
    if(TransformUpdater != NULL) {
        TransformUpdater->addPointItem(item);
    }

    return item;
}

/**
  Called when a point has been added
  */
void cwAbstractPointManager::pointAdded() {
    createComponent();

    if(PointComponent == NULL) {
        qDebug() << "StationItemComponent bad" << LOCATION;
        return;
    }

    //Create a new station item
    QQuickItem* item = addItem();

    //Update the station
    if(item != NULL) {
        int lastIndex = PointItems.size() - 1;
        updateStationItemData(item, lastIndex);
    }
}

/**
  Handles when the point is removed
  */
void cwAbstractPointManager::stationRemoved(int index) {
    //Unselect the item that's going to be deleted
    if(index >= 0 || index < PointItems.size()) {
        if(selectedindex() == index) {
            clearSelection();
        }

        PointItems[index]->deleteLater();
        PointItems.removeAt(index);

        updateAllData();
    }
}

/**
  \brief Updates the station's position
  */
void cwAbstractPointManager::udpateStationPosition(int stationIndex) {
    if(Scrap == NULL) { return; }
    if(stationIndex >= 0 || stationIndex < StationItems.size()) {
        QPointF point = Scrap->stationData(cwScrap::StationPosition, stationIndex).value<QPointF>();
        StationItems[stationIndex]->setProperty("position3D", QVector3D(point));

        if(stationIndex == selectedStationIndex()) {
            updateShotLines();
        }
    }
}

