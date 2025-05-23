/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurveyChunkGroupView.h"
#include "cwTrip.h"
#include "cwSurveyChunkView.h"
#include "cwSurveyChunk.h"
#include "cwSurveyChunkViewComponents.h"
#include "cwTripCalibration.h"
#include "cwDebug.h"
#include "cwSurveyChunkTrimmer.h"

//Qt includes
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQmlContext>
#include <QKeyEvent>

cwSurveyChunkGroupView::cwSurveyChunkGroupView(QQuickItem *parent) :
    QQuickItem(parent),
    Trip(nullptr),
    ChunkQMLComponents(nullptr),
    NeedChunkAboveMapper(new QSignalMapper(this)),
    NeedChunkBelowMapper(new QSignalMapper(this)),
    ChunkTrimmer(new cwSurveyChunkTrimmer(this))
{
    connect(NeedChunkAboveMapper, &QSignalMapper::mappedInt, this, &cwSurveyChunkGroupView::forceAllocateChunkAbove);
    connect(NeedChunkBelowMapper, &QSignalMapper::mappedInt, this, &cwSurveyChunkGroupView::forceAllocateChunkBelow);
}

void cwSurveyChunkGroupView::setTrip(cwTrip* trip) {
    if(ChunkQMLComponents == nullptr) {
        QQmlContext* context = QQmlEngine::contextForObject(this);
        context->contextProperty("globalShadowTextInput");
        ChunkQMLComponents = new cwSurveyChunkViewComponents(context, this);
    }

    if(Trip != trip) {
        if(Trip != nullptr) {
            disconnect(Trip, nullptr, this, nullptr);
        }

        Trip = trip;

        if(Trip != nullptr) {
            //Clear the current view
            foreach(cwSurveyChunkView* view, ChunkViews) {
                if(view != nullptr) {
                    view->deleteLater();
                }
            }
            ChunkViews.clear();
            ChunkBoundingRects.clear();


            if(Trip != nullptr) {
                //Add chunks to the view
                addChunks(0, Trip->chunkCount() - 1);

                connect(Trip, SIGNAL(chunksInserted(int,int)), SLOT(addChunks(int,int)));
                connect(Trip, SIGNAL(chunksRemoved(int,int)), SLOT(removeChunks(int,int)));
                connect(Trip->calibrations(), SIGNAL(frontSightsChanged()), SLOT(updateChunksFrontSights()));
                connect(Trip->calibrations(), SIGNAL(backSightsChanged()), SLOT(updateChunksBackSights()));
            }

            emit tripChanged();
            emit contentHeightChanged();
            emit contentWidthChanged();
        }
    }
}

cwTrip* cwSurveyChunkGroupView::trip() const {
    return Trip;
}

/**
  Sets the viewports x position
  */
void cwSurveyChunkGroupView::setViewportX(float x) {
    if(ViewportArea.x() != x) {
        ViewportArea.moveLeft(x);
        emit viewportXChanged();
    }
}

/**
  Sets the viewports y position
  */
void cwSurveyChunkGroupView::setViewportY(float y) {
    if(ViewportArea.y() != y) {
        //qDebug() << "Set viewport y:" << y;
        ViewportArea.moveTop(y - this->y());
        updateActiveChunkViews();
        emit viewportYChanged();
    }
}

/**
  Sets the viewports width
  */
void cwSurveyChunkGroupView::setViewportWidth(float width) {
    if(ViewportArea.width() != width) {
       // qDebug() << "Set viewport width" << width;
        ViewportArea.setWidth(width);
        emit viewportWidthChanged();
    }
}

/**
  Sets the viewports height
  */
void cwSurveyChunkGroupView::setViewportHeight(float height) {
    if(ViewportArea.height() != height) {
       // qDebug() << "Set viewport height: " << height;
        ViewportArea.setHeight(height);
        updateActiveChunkViews();
        emit viewportHeightChanged();
    }
}

/**
  The key event that's pressed
  */
void cwSurveyChunkGroupView::keyPressEvent(QKeyEvent *event) {
    if(event->key() == Qt::Key_Space) {
        //Add a new survey chunk
        //Split current item



        //ChunkGroup->insertRow(ChunkGroup->rowCount());
    }

    event->accept();
}

/**
  \brief The bounding rect for the view
  */
//QRectF cwSurveyChunkGroupView::boundingRect() const {
//    if(parentItem() != nullptr) {
//        return parentItem()->boundingRect();
//    } else {
//        return QRectF(0, 0, width(), height());
//    }
//}

/**
  \brief Updates active chunks

  This will check what chunks should be shown, and which chunk should be hidden
  The hidden chunks will be deleted (exept if whitelisted) New views may be created, if nessary
  */
void cwSurveyChunkGroupView::updateActiveChunkViews() {
    QPair<int, int> range = VisableRange();
    if(range.first < 0 || range.first >= ChunkViews.size()) { return; }
    if(range.second < 0 || range.second >= ChunkViews.size()) { return; }

    float oldContentWidth = childrenRect().width();

    //qDebug() << "Range: " << range;
    //Remove views going in the negative direction
    for(int i = range.first - 1; i >= 0; i--) {
        DeleteChunkView(i);
    }

    //Remove views going in the positive direction
    for(int i = range.second + 1; i < ChunkViews.size(); i++) {
        DeleteChunkView(i);
    }

    //Create views that don't exist
    for(int i = range.first; i <= range.second; i++) {
        //Remove elements from white list
        ChunkViewsWhiteList.remove(i);

        //Create the chunk view
        CreateChunkView(i);
    }

    //Update the positions off all the visible elements
    //and the navigation
    for(int i = range.first; i <= range.second; i++) {
        updateAboveBelowAndPosition(i);
    }

    if(oldContentWidth != contentWidth()) {
        emit contentWidthChanged();
    }
}

bool lessThanChunkRect(QRectF first, QRectF second) {
    return first.y() < second.y();
}

/**
  \brief Get's the visable range of which chunks should be shown
  */
QPair<int, int> cwSurveyChunkGroupView::VisableRange() {
    QList<QRectF>::iterator firstIter = std::lower_bound(ChunkBoundingRects.begin(), ChunkBoundingRects.end(), QRectF(ViewportArea.topLeft(), QSize(1, 1)), lessThanChunkRect);
    QList<QRectF>::iterator lastIter = std::lower_bound(ChunkBoundingRects.begin(), ChunkBoundingRects.end(), QRectF(ViewportArea.bottomLeft(), QSize(1, 1)), lessThanChunkRect);

   // qDebug() << "First: " << firstIter - ChunkBoundingRects.begin() << " Second: " << lastIter - ChunkBoundingRects.begin() << ViewportArea;
//    if(firstIter != ChunkBoundingRects.end()) {
//        qDebug() << "Viewport:" << ViewportArea << "Chunk" << *firstIter;
//    }

    if(firstIter <= lastIter) {
        if(firstIter != ChunkBoundingRects.begin()) { firstIter--; }
        if(lastIter == ChunkBoundingRects.end()) { lastIter--; }
        return QPair<int, int>(firstIter - ChunkBoundingRects.begin(), lastIter - ChunkBoundingRects.begin());
    } else {
        return QPair<int, int>(-1, -1);
    }

}

/**
  \brief Creates a chunk view at index

  This doesn't insert a new chunkview at index, it simply creates a new view at
  that position.  The position must already exist
  */
void cwSurveyChunkGroupView::CreateChunkView(int index) {
    if(index < 0 || index >= ChunkViews.size()) { return; }
    if(ChunkViews[index] == nullptr) {

         cwSurveyChunk* chunk = Trip->chunk(index);

        //Create a chunkView object
        cwSurveyChunkView* chunkView = new cwSurveyChunkView(this);
        chunkView->setErrorButtonGroup(m_errorButtonGroup);

        QQmlContext* context = QQmlEngine::contextForObject(this);
        QQmlEngine::setContextForObject(chunkView, context);
        chunkView->setQMLComponents(ChunkQMLComponents);

        //Set the chunkview rendering frontSights or backsights
        chunkView->setFrontSights(Trip->calibrations()->hasFrontSights());
        chunkView->setBackSights(Trip->calibrations()->hasBackSights());

        //Set the trimmer
        chunkView->setChunkTrimmer(ChunkTrimmer);

        //Set the model for the view
        chunkView->setModel(chunk);

        //Make sure we update when the chunkHasChanged
        connect(chunkView, SIGNAL(heightChanged()), SLOT(UpdateChunkHeight()));

        //Updates the model when items are split
        connect(chunkView, SIGNAL(createdNewChunk(cwSurveyChunk*)), SLOT(HandleSplitChunk(cwSurveyChunk*)));

        //Make sure the chunkView keeps the visiblity for items when focused
        connect(chunkView, SIGNAL(ensureVisibleChanged(QRectF)), SLOT(SetEnsureVisibleRect(QRectF)));

        //Connect the force allocation for the chunk
        connect(chunkView, SIGNAL(needChunkAbove()), NeedChunkAboveMapper, SLOT(map()));
        NeedChunkAboveMapper->setMapping(chunkView, index);
        connect(chunkView, SIGNAL(needChunkBelow()), NeedChunkBelowMapper, SLOT(map()));
        NeedChunkBelowMapper->setMapping(chunkView, index);

        ChunkViews[index] = chunkView;
        //qDebug() << "Chunk Bounding box:" << index << chunkView->boundingRect();
    }
}

/**
  \brief Deletes the chunk from the view

  This doesn't remove a chunk view at the index, it simply deletes the view and
  sets the index to nullptr
  */
void cwSurveyChunkGroupView::DeleteChunkView(int index) {
    if(ChunkViews[index] != nullptr && !ChunkViewsWhiteList.contains(index)) {
//        qDebug() << "Deleting" << ChunkViews[index];
        ChunkViews[index]->deleteLater();
        ChunkViews[index] = nullptr;
    }
}

/**
 * @brief cwSurveyChunkGroupView::spacing
 *
 * Returns the spacing between survey chunk group views
 */
double cwSurveyChunkGroupView::spacing() const
{
    return 1; //1 pixels
}

/**
  \brief Sets the focus for a view at index

  This will cause a view to become focused
  */
void cwSurveyChunkGroupView::setFocus(int index) {
    if(index < 0 || index >= ChunkViews.size()) { return; }
    CreateChunkView(index); //Make sure the chunk exists;
    cwSurveyChunkView* surveyChunk = ChunkViews.at(index);
    surveyChunk->setFocus(true);
}

/**
  \brief Sets the chunk's above and below chunk

  This also set's the chunk's bounding rect
  */
void cwSurveyChunkGroupView::updateAboveBelowAndPosition(int index) {
    ChunkViews[index]->setY(ChunkBoundingRects[index].top());

    //Update the navigation for the chunk
    cwSurveyChunkView* aboveChunk = nullptr;
    cwSurveyChunkView* belowChunk = nullptr;

    int aboveIndex = index - 1;
    int belowIndex = index + 1;

    if(aboveIndex >= 0 && aboveIndex < ChunkViews.size()) {
        aboveChunk = ChunkViews[aboveIndex];
    }

    if(belowIndex >= 0 && belowIndex < ChunkViews.size()) {
        belowChunk = ChunkViews[belowIndex];
    }

    ChunkViews[index]->setNavigation(aboveChunk, belowChunk);
}

/**
  \brief Insert rows
  */
//void cwSurveyChunkGroupView::InsertRows(int start, int end) {
//    AddChunks(start, end);
//}

/**
  Adds new chunks to the view from beginIndex to endIndex
  */
void cwSurveyChunkGroupView::addChunks(int beginIndex, int endIndex) {
    if(Trip == nullptr) { return; }
    ///qDebug() << "Adding chunks " << beginIndex << endIndex;

    for(int i = beginIndex; i <= endIndex; i++) {
        cwSurveyChunk* chunk = Trip->chunk(i);

        if(chunk == nullptr) { continue; }

        ChunkViews.insert(i, nullptr);
        ChunkBoundingRects.insert(i, QRectF());
    }

    updateContentArea(beginIndex, endIndex);
    updateActiveChunkViews();

    //If only one chunk is added set the focus on it
    if(beginIndex == endIndex) {
        setFocus(beginIndex);
    }
}

/**
 * @brief cwSurveyChunkGroupView::removeChunks
 * @param beginIndex
 * @param endIndex
 *
 * Removes the chunks between beginIndex and endIndex
 */
void cwSurveyChunkGroupView::removeChunks(int beginIndex, int endIndex)
{
    if(Trip == nullptr) { return; }

    for(int i = beginIndex; i <= endIndex; i++) {
        ChunkViews.at(i)->deleteLater();
        ChunkViews.removeAt(i);
        ChunkBoundingRects.removeAt(i);
    }

    updateContentArea(beginIndex, endIndex);
    updateActiveChunkViews();

    if(ChunkViews.isEmpty()) {
        emit contentHeightChanged();
        emit contentWidthChanged();
    }
}

/**
  Updates the chunk's height

  This is used with a signalling cwSurveyChunkView that thier height has changed
  */
void cwSurveyChunkGroupView::UpdateChunkHeight() {
    cwSurveyChunkView* chunkView = qobject_cast<cwSurveyChunkView*>(sender());
    if(chunkView != nullptr) {
        int index = ChunkViews.indexOf(chunkView);
        updateContentArea(index, ChunkViews.size() - 1);
        updateActiveChunkViews();
    }
}

/**
  \brief Ensure that rect is visible

  This emit a signal such that, qml flickable area will scroll such that the rect
  will be in the visible area
  */
void cwSurveyChunkGroupView::SetEnsureVisibleRect(QRectF rect) {
    float extraHeight = cwSurveyChunkView::elementHeight() * 3;
    rect.setHeight(rect.height() + extraHeight);
    rect.moveTop(rect.top() - extraHeight / 2.0);

    EnsureVisibleArea = mapRectToItem(parentItem(), rect);
    emit ensureVisibleRectChanged();
}

/**
  \brief This adds split chunk into the model

  newChunk is the new chunk that needs to be added to the model.  This happens when
  the chunk has been split into two chunks, by the user
  */
void cwSurveyChunkGroupView::HandleSplitChunk(cwSurveyChunk *newChunk) {

    cwSurveyChunkView* chunkView = qobject_cast<cwSurveyChunkView*>(sender());
    if(chunkView == nullptr) { return; }

    //Find the chunk
    int firstIndex = VisableRange().first; //Only the visible range is valid
    int insertIndex = ChunkViews.indexOf(chunkView, firstIndex) + 1;

    Trip->insertChunk(insertIndex, newChunk);
}

/**
    \brief Updates all the chunks so they can either render with front sights or not
  */
void cwSurveyChunkGroupView::updateChunksFrontSights() {
    foreach(cwSurveyChunkView* chunkView, ChunkViews) {
        if(chunkView != nullptr) {
            chunkView->setFrontSights(Trip->calibrations()->hasFrontSights());
        }
    }
}

/**
   \brief Updates all the chunks so they can either render with back sights or not
  */
void cwSurveyChunkGroupView::updateChunksBackSights() {
    foreach(cwSurveyChunkView* chunkView, ChunkViews) {
        if(chunkView != nullptr) {
            chunkView->setBackSights(Trip->calibrations()->hasBackSights());
        }
    }
}

/**
  This will try to create the chunk above index, if it doesn't already exist.

  The created chunk will be blacklisted, if it has been created.  What happens is
  is the chunkview hasn't been created yet, because it's note visible to the user.
  This is mostly to support keyboard interaction (Tabs and arrows).  Once the scrap
  is visible, it'll be removed from the blacklist. The blacklist prevents the scrap
  from being delete when it's not being shown.

  This will also update the chunk at index, with the new allocated scrap
  */
void cwSurveyChunkGroupView::forceAllocateChunkAbove(int index) {
    if(index > 0) {
        int chunkAbove = index - 1;
        forceAllocateChunk(index, chunkAbove);
    }
}

/**
  This will try to create the chunk below index, if it doesn't already exist.

  The created chunk will be blacklisted, if it has been created.  What happens is
  is the chunkview hasn't been created yet, because it's note visible to the user.
  This is mostly to support keyboard interaction (Tabs and arrows).  Once the scrap
  is visible, it'll be removed from the blacklist. The blacklist prevents the scrap
  from being delete when it's not being shown
  */
void cwSurveyChunkGroupView::forceAllocateChunkBelow(int index) {
    if(index < ChunkViews.size() - 1) {
        int chunkBelow = index + 1;
        forceAllocateChunk(index, chunkBelow);
    }
}

/**
  This will try to create the chunk at allocatedChunkIndex, if it doesn't already exist.

  The created chunk will be blacklisted, if it has been created.  What happens is
  is the chunkview hasn't been created yet, because it's note visible to the user.
  This is mostly to support keyboard interaction (Tabs and arrows).  Once the scrap
  is visible, it'll be removed from the blacklist. The blacklist prevents the scrap
  from being delete when it's not being shown
  */
void cwSurveyChunkGroupView::forceAllocateChunk(int chunkIndex, int allocatedChunkIndex) {
    if(ChunkViews[allocatedChunkIndex] != nullptr) {
        qDebug() << "Force allocated ChunkView alread exist at:" << allocatedChunkIndex << LOCATION;
        return;
    }

    ChunkViewsWhiteList.insert(allocatedChunkIndex);
    CreateChunkView(allocatedChunkIndex);

    //Update the below and above
    updateAboveBelowAndPosition(allocatedChunkIndex);
    updateAboveBelowAndPosition(chunkIndex);
}

/**
  Updates the surveychunkview's position at index
  */
//void cwSurveyChunkGroupView::UpdatePosition(int index) {

//    int previousIndex = index - 1;
//    if(previousIndex < 0) {
//        ChunkViews[index]->setPosition(QPointF(0.0f, 0.0f));
//        return;
//    }

//    cwSurveyChunkView* previousView = ChunkViews[previousIndex];
//    //const float buffer = 10.0;
//    float x = 0.0f;
//    float y = mapRectFromItem(previousView, previousView->boundingRectangle()).bottom() + spacing(); //height(); // + buffer;

//    cwSurveyChunkView* view = ChunkViews[index];
//    view->setPosition(QPointF(x, y));
//}

/**
  \brief Updates the ChunkBoundingRects and ChunkYPosition between and include the index
  of beginIndex and endIndex

  Will update the interal positions of those chunks, this allows the caching of elements
  */
void cwSurveyChunkGroupView::updateContentArea(int beginIndex, int endIndex) {
    //Make sure beginIndex and endIndex are good
    if(beginIndex < 0 || beginIndex >= Trip->chunkCount()) { return; }
    if(endIndex < 0 || endIndex >= Trip->chunkCount()) { return; }
    if(beginIndex > endIndex) { return; }

    //Calc the yOffset
    float yOffset;
    int beforeIndex = beginIndex - 1;
    if(beforeIndex >= 0) {
        QRectF beforeRect = ChunkBoundingRects[beforeIndex];
        yOffset = beforeRect.bottom() + spacing();
    } else {
        yOffset = 0;
    }

    for(int i = beginIndex; i <= endIndex; i++) {
        cwSurveyChunk* chunk = Trip->chunk(i);;
        if(chunk == nullptr) { continue; }

        float height = cwSurveyChunkView::heightHint(chunk->stationCount());
        ChunkBoundingRects[i].setHeight(height);
        ChunkBoundingRects[i].setWidth(1);
        ChunkBoundingRects[i].moveTop(yOffset);
        yOffset = ChunkBoundingRects[i].bottom() + spacing();
    }

    //Update the reminding chunks below
    for(int i = endIndex + 1; i < ChunkBoundingRects.size(); i++) {
        ChunkBoundingRects[i].moveTop(yOffset);
        yOffset = ChunkBoundingRects[i].bottom() + spacing();
    }

    //Update the positions off all the visible elements
    for(int i = beginIndex; i < ChunkViews.size(); i++) {
        if(ChunkViews[i] != nullptr) {
            ChunkViews[i]->setY(ChunkBoundingRects[i].top());
        }
    }

    emit contentHeightChanged();
}

/**
  \brief Get's the content's height of the view

  This is the height if all the elements where displayed at once
  */
double cwSurveyChunkGroupView::contentHeight() const {
    if(ChunkBoundingRects.isEmpty()) { return 0.0; }
    return ChunkBoundingRects.last().bottom() - ChunkBoundingRects.first().top() + 1.0;
}

double cwSurveyChunkGroupView::contentWidth() {
    if(ChunkBoundingRects.isEmpty()) { return 392.141; }
//    qDebug() << "Children width: " << childrenRect().width();
    return childrenRect().width();
}



QObject *cwSurveyChunkGroupView::errorButtonGroup() const
{
    return m_errorButtonGroup;
}

void cwSurveyChunkGroupView::setErrorButtonGroup(QObject *newErrorButtonGroup)
{
    if (m_errorButtonGroup == newErrorButtonGroup) {
        return;
    }
    m_errorButtonGroup = newErrorButtonGroup;

    for(auto chunkView : ChunkViews) {
        chunkView->setErrorButtonGroup(m_errorButtonGroup);
    }

    emit errorButtonGroupChanged();
}
