/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLabel3dView.h"
#include "cwGlobalDirectory.h"
#include "cwLabel3dItem.h"
#include "cwCamera.h"
#include "cwDebug.h"
#include "cwLabel3dGroup.h"

//Qt includes
#include <QQmlContext>
#include <QQmlEngine>
#include <QtConcurrent>

cwLabel3dView::cwLabel3dView(QQuickItem *parent) :
    QQuickItem(parent),
    Component(nullptr),
    Camera(nullptr)
{
    connect(this, &cwLabel3dView::visibleChanged, this, [this]() {
        if(isVisible()) {
            updatePositions();
        }
    });
}

/**
 * @brief cwLabel3dView::~cwLabel3dView
 *
 * Delete all the 3d view data
 */
cwLabel3dView::~cwLabel3dView()
{
//    for(auto groups : LabelGroups) {
//        groups->setParentView(nullptr);
//    }
//    //Delete all the child groups
//    QSetIterator<cwLabel3dGroup*> iter(LabelGroups);
//    while(iter.hasNext()) {
//        cwLabel3dGroup* group = iter.next();
//        group->Labels.clear();
//        group->setParentView(nullptr);
//        group->deleteLater();
//    }
}

void cwLabel3dView::addGroup(cwLabel3dGroup *group) {
    if(!LabelGroups.contains(group)) {
        LabelGroups.insert(group);
        group->setParentView(this);
        updateGroup(group);
    }
}

void cwLabel3dView::removeGroup(cwLabel3dGroup *group) {
    if(LabelGroups.contains(group)) {
        LabelGroups.remove(group);
        group->setParentView(nullptr);
    }
}

/**
  * @brief updateGroup
  * @param group
  *
  * Updates the group's QQuickItem's. This will remove old label, create new ones, as needed, and
  * update the text and font properties.
  */
void cwLabel3dView::updateGroup(cwLabel3dGroup* group) {
    if(Component == nullptr) {
        //Create the component that will generate all the labels
        QQmlEngine* engine = QQmlEngine::contextForObject(this)->engine();
        Component = new QQmlComponent(engine,  "qrc:/cavewherelib/cavewherelib/Label3d.qml", this);

        if(Component == nullptr) {
            qDebug() << "Component is nullptr" << LOCATION;
        }

        if(!Component->errors().isEmpty()) {
            qDebug() << Component->errorString();
        }
    }

    Q_ASSERT(LabelGroups.contains(group));

    //Remove extra
    int numberExtra = group->LabelItems.size() - group->Labels.size();
    for(int i = 0; i < numberExtra; i++) {
        group->LabelItems.last()->deleteLater();
        group->LabelItems.removeLast();
    }

    //Add extra
    numberExtra = group->Labels.size() - group->LabelItems.size();
    for(int i = 0; i < numberExtra; i++) {
        QQuickItem* newItem = qobject_cast<QQuickItem*>(Component->create());
        newItem->setParent(group);
        newItem->setParentItem(this);
        group->LabelItems.append(newItem);
    }

    //Update all the info for the label
    Q_ASSERT(group->Labels.size() == group->LabelItems.size());
    for(int i = 0; i < group->Labels.size(); i++) {
        const cwLabel3dItem& label = group->Labels.at(i);
        QQuickItem* item = group->LabelItems.at(i);

        item->setProperty("text", label.text());
    }

    //Update all the positions
    updateGroupPositions(group);
}

/**
Sets camera
*/
void cwLabel3dView::setCamera(cwCamera* camera) {
    if(Camera != camera) {
        Camera = camera;
        connect(Camera, &cwCamera::viewMatrixChanged, this, &cwLabel3dView::updatePositions);
        connect(Camera, &cwCamera::projectionChanged, this, &cwLabel3dView::updatePositions);
        updatePositions();
        emit cameraChanged();
    }
}

/**
 * @brief cwLabel3dView::updatePositions
 *
 * Updates all the positions for the view.
 *
 * If camera is null, this does nothing
 */
void cwLabel3dView::updateGroupPositions(cwLabel3dGroup* group)
{

    Q_ASSERT(group->Labels.size() == group->LabelItems.size());

    //Copy all the labels
    QList<cwLabel3dItem> labels = group->Labels;

    //Transforms all the label's points
    QtConcurrent::blockingMap(labels,
                              TransformPoint(Camera->viewProjectionMatrix(),
                                             Camera->viewport()));

    //Go through all the station points and render the text
    for(int i = 0; i < labels.size(); i++) {
        const cwLabel3dItem& label = labels.at(i);
        QQuickItem* item = group->LabelItems.at(i);

        QVector3D projectedStationPosition = label.position();



        //Clip the stations to the rendering area
        if(projectedStationPosition.z() > 1.0 ||
                projectedStationPosition.z() < 0.0 ||
                !Camera->viewport().contains(projectedStationPosition.x(), projectedStationPosition.y())) {
            item->setVisible(false);
            continue;
        }

        //See if stationName overlaps with other stations
        QPoint topLeftPoint = projectedStationPosition.toPoint();
        QSize stationNameTextSize(item->width() * 1.1, item->height() * 1.1);
        QRect stationRect(topLeftPoint, stationNameTextSize);
        stationRect.moveTop(stationRect.top() - stationNameTextSize.height() / 1.1);
        bool couldAddText = LabelKdTree.addRect(stationRect);

        if(couldAddText) {
            item->setVisible(true);
            item->setPosition(projectedStationPosition.toPointF());
        } else {
            item->setVisible(false);
        }
    }
}

/**
 * @brief cwLabel3dView::updatePositions
 *
 * Go through each group and update the positions
 */
void cwLabel3dView::updatePositions()
{
    if(Camera == nullptr) { return; }

    if(isVisible()) {
        QSetIterator<cwLabel3dGroup*> iter(LabelGroups);
        while(iter.hasNext()) {
            cwLabel3dGroup* group = iter.next();
            updateGroupPositions(group);
        }

        LabelKdTree.clear();
    }
}

/**
  \brief This is a helper for QtCurrentent function

  This is the kernel for multi threaded algroithm to transform the points into
  screen coordinates.  This is a helper function to renderStationLabels
  */
void cwLabel3dView::TransformPoint::operator()(cwLabel3dItem& label) {
    QVector3D normalizeSceenCoordinate =  ModelViewProjection * label.position();
    QVector3D viewportCoord = cwCamera::mapNormalizeScreenToGLViewport(normalizeSceenCoordinate, Viewport);
    float y = Viewport.y() + (Viewport.height() - viewportCoord.y());
    viewportCoord.setY(y);
    label.setPosition(viewportCoord);
}
