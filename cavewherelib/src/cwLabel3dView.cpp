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
#include <algorithm>
#include <QRectF>
#include <QSizeF>
cwLabel3dView::cwLabel3dView(QQuickItem *parent) :
    QQuickItem(parent),
    m_component(nullptr),
    m_camera(nullptr)
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
    if(!m_labelGroups.contains(group)) {
        m_labelGroups.insert(group);
        group->setParentView(this);
        updateGroup(group);
    }
}

void cwLabel3dView::removeGroup(cwLabel3dGroup *group) {
    if(m_labelGroups.contains(group)) {
        m_labelGroups.remove(group);
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
    if(m_component == nullptr) {
        //Create the component that will generate all the labels
        QQmlEngine* engine = QQmlEngine::contextForObject(this)->engine();
        m_component = new QQmlComponent(engine,  "qrc:/qt/qml/cavewherelib/qml/Label3d.qml", this);

        if(m_component == nullptr) {
            qDebug() << "Component is nullptr" << LOCATION;
        }

        if(!m_component->errors().isEmpty()) {
            qDebug() << m_component->errorString();
        }
    }

    Q_ASSERT(m_labelGroups.contains(group));

    const int labelSize = group->m_labels.size();
    const int itemSize = group->m_labelItems.size();

    if(labelSize < itemSize) {
        for(int i = labelSize; i < itemSize; i++) {
            releaseLabelItem(group, i);
        }
        group->m_labelItems.resize(labelSize);
    } else if(labelSize > itemSize) {
        group->m_labelItems.resize(labelSize);
    }

    //Update all the positions
    updateGroupPositions(group);
}

QQuickItem* cwLabel3dView::labelItem(cwLabel3dGroup* group, int labelIndex)
{
    if(group == nullptr) {
        return nullptr;
    }
    if(labelIndex < 0 || labelIndex >= group->m_labelItems.size()) {
        return nullptr;
    }
    return group->m_labelItems.at(labelIndex);
}

QQuickItem* cwLabel3dView::acquireLabelItem(cwLabel3dGroup* group, int labelIndex)
{
    if(group == nullptr) {
        return nullptr;
    }
    if(labelIndex < 0 || labelIndex >= group->m_labelItems.size()) {
        return nullptr;
    }

    if(labelItem(group, labelIndex) != nullptr) {
        return group->m_labelItems.at(labelIndex);
    }

    if(m_component == nullptr) {
        QQmlEngine* engine = QQmlEngine::contextForObject(this)->engine();
        m_component = new QQmlComponent(engine, "qrc:/qt/qml/cavewherelib/qml/Label3d.qml", this);

        if(m_component == nullptr) {
            qDebug() << "Component is nullptr" << LOCATION;
        }

        if(!m_component->errors().isEmpty()) {
            qDebug() << m_component->errorString();
        }
    }

    QQuickItem* item = nullptr;
    if(!group->m_itemPool.isEmpty()) {
        item = group->m_itemPool.takeLast();
    } else {
        item = qobject_cast<QQuickItem*>(m_component->create());
    }

    if(item == nullptr) {
        qDebug() << "Problem creating new label item" << LOCATION;
        return nullptr;
    }

    item->setParent(group);
    item->setParentItem(this);
    item->setVisible(false);
    group->m_labelItems[labelIndex] = item;
    return item;
}

void cwLabel3dView::releaseLabelItem(cwLabel3dGroup* group, int labelIndex)
{
    if(group == nullptr) {
        return;
    }
    if(labelIndex < 0 || labelIndex >= group->m_labelItems.size()) {
        return;
    }

    QQuickItem* item = group->m_labelItems.at(labelIndex);
    if(item == nullptr) {
        return;
    }

    item->setVisible(false);
    item->setOpacity(0.0);
    group->m_itemPool.append(item);
    group->m_labelItems[labelIndex] = nullptr;
}

/**
Sets camera
*/
void cwLabel3dView::setCamera(cwCamera* camera) {
    if(m_camera != camera) {
        m_camera = camera;
        connect(m_camera, &cwCamera::viewMatrixChanged, this, &cwLabel3dView::updatePositions);
        connect(m_camera, &cwCamera::projectionChanged, this, &cwLabel3dView::updatePositions);
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
    if(m_camera == nullptr) {
        return;
    }

    Q_ASSERT(group->m_labels.size() == group->m_labelItems.size());

    QList<cwLabel3dItem>& labels = group->m_labels;

    const QMatrix4x4 matrix = m_camera->qtViewportMatrix() * m_camera->viewProjectionMatrix();
    const QRectF viewportRect(m_camera->viewport());
    const QSizeF averageSize = m_averageLabelSize;

    const int labelCount = labels.size();
    constexpr int visibilityThreshold = 4;

    auto processIndex = [&](int i) {
        QQuickItem* item = labelItem(group, i);
        cwLabel3dItem& label = labels[i];
        QVector3D qtViewportCoordinate = matrix.map(label.position());

        auto increaseVisiblity = [&]() {
            label.setVisibleStreak(std::min(label.visibleStreak() + 1, visibilityThreshold));
            // label.setPriority(label.priority() + 1);
            // label.setVisibleStreak(label.visibleStreak() + 1);
        };

        auto accept = [&]() {
            increaseVisiblity();

            if(label.visibleStreak() >= visibilityThreshold) {
                if(item == nullptr) {
                    item = acquireLabelItem(group, i);
                    item->setProperty("text", label.text());
                    item->setVisible(true);
                }
            }

            if (item) {
                item->setPosition(qtViewportCoordinate.toPointF());
            }
        };

        auto reject = [&]() {
            label.setVisibleStreak(0);
            if(item && item->isVisible()) {
                releaseLabelItem(group, i);
            }
        };

        if(m_camera->isQtViewportCoordinateClipped(qtViewportCoordinate)) {
            reject();
            return;
        }

        //See if stationName overlaps with other stations
        QPointF topLeftPoint(qtViewportCoordinate.x(), qtViewportCoordinate.y());
        QSizeF stationNameTextSize = averageSize;
        if(item) {
            const double width = item->width();
            const double height = item->height();
            if(width > 0.0 && height > 0.0) {
                stationNameTextSize.setWidth(width);
                stationNameTextSize.setHeight(height);
            }
        }

        constexpr double margin = 1.1;
        stationNameTextSize = stationNameTextSize * margin;
        QRectF stationRect(topLeftPoint, stationNameTextSize);
        stationRect.moveTop(stationRect.top() - stationNameTextSize.height() / margin);
        bool couldAddText = m_labelKdTree.addRect(stationRect.toAlignedRect());

        if(couldAddText) {
            accept();
        } else {
            reject();
        }

        //Debugging
        // if(item) {
        //     item->setProperty("text", QString("%1 a:%2 p:%3")
        //                                   .arg(label.text())
        //                                   .arg(label.visibleStreak())
        //                                   .arg(label.priority()));
        // }
    };

    auto sortVisibleEntries = [&]() {
        group->m_visibleEntries.reserve(labelCount);
        group->m_visibleEntries.clear();
        for(int i = 0; i < labelCount; i++) {
            if(labels.at(i).wasVisible()) {
                QVector3D qtViewportCoordinate = matrix.map(labels.at(i).position());
                group->m_visibleEntries.emplaceBack(i,
                                                    labels.at(i).visibleStreak(),
                                                    qtViewportCoordinate);
            }
        }

        std::sort(group->m_visibleEntries.begin(), group->m_visibleEntries.end(),
                  [&](const auto& lhs, const auto& rhs) {
                      if(lhs.priority != rhs.priority) {
                          return lhs.priority > rhs.priority;
                      }
                      if(lhs.position.z() != rhs.position.z()) {
                          return lhs.position.z() < rhs.position.z();
                      }
                      if(lhs.position.y() != rhs.position.y()) {
                          return lhs.position.y() < rhs.position.y();
                      }
                      if(lhs.position.x() != rhs.position.x()) {
                          return lhs.position.x() < rhs.position.x();
                      }
                      return lhs.index < rhs.index;
                  });
    };

    sortVisibleEntries();

    //Go through all the station points and render the text (visible items first)
    for(int i = 0; i < group->m_visibleEntries.size(); i++) {
        processIndex(group->m_visibleEntries.at(i).index);
    }

    for(int i = 0; i < labelCount; i++) {
        if(!labels.at(i).wasVisible()) {
            processIndex(i);
        }
    }

    double widthSum = 0.0;
    double heightSum = 0.0;
    int sizeCount = 0;
    for(int i = 0; i < labelCount; i++) {
        QQuickItem* item = group->m_labelItems.at(i);
        const bool isVisible = item != nullptr && item->isVisible();
        labels[i].setWasVisible(isVisible ? 1 : 0);
        if(isVisible) {
            const double width = item->width();
            const double height = item->height();
            if(width > 0.0 && height > 0.0) {
                widthSum += width;
                heightSum += height;
                sizeCount++;
            }
        }
    }

    if(sizeCount > 0) {
        m_averageLabelSize.setWidth(widthSum / sizeCount);
        m_averageLabelSize.setHeight(heightSum / sizeCount);
    }
}

/**
 * @brief cwLabel3dView::updatePositions
 *
 * Go through each group and update the positions
 */
void cwLabel3dView::updatePositions()
{
    if(m_camera == nullptr) { return; }

    if(isVisible()) {
        QSetIterator<cwLabel3dGroup*> iter(m_labelGroups);
        while(iter.hasNext()) {
            cwLabel3dGroup* group = iter.next();
            updateGroupPositions(group);
        }

        m_labelKdTree.clear();
    }
}

/**
Gets camera
*/
cwCamera *cwLabel3dView::camera() const {
    return m_camera;
}
