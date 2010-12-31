//Our includes
#include "cwSurveyChunkGroupView.h"
#include "cwSurveyTrip.h"
#include "cwSurveyChuckView.h"
#include "cwSurveyChunk.h"
#include "cwSurveyChunkViewComponents.h"

//Qt includes
#include <QDeclarativeEngine>
#include <QDeclarativeComponent>
#include <QDeclarativeItem>
#include <QDeclarativeContext>
#include <QKeyEvent>

cwSurveyChunkGroupView::cwSurveyChunkGroupView(QDeclarativeItem *parent) :
    QDeclarativeItem(parent),
    Trip(NULL),
    ChunkQMLComponents(NULL)
{
}

void cwSurveyChunkGroupView::setChunkGroup(cwTrip* trip) {
    if(ChunkQMLComponents == NULL) {
        QDeclarativeContext* context = QDeclarativeEngine::contextForObject(this);
        ChunkQMLComponents = new cwSurveyChunkViewComponents(context, this);
    }

    if(Trip != trip) {
        Trip = trip;

        //Clear the current view
        foreach(cwSurveyChunkView* view, ChunkViews) {
            if(view != NULL) {
                view->deleteLater();
            }
        }
        ChunkViews.clear();
        ChunkBoundingRects.clear();


        //Add chunks to the view
        AddChunks(0, Trip->rowCount() - 1);

        emit chunkGroupChanged();

        connect(Trip, SIGNAL(rowsInserted(QModelIndex, int, int)), SLOT(InsertRows(QModelIndex,int,int)));
    }
}

cwTrip* cwSurveyChunkGroupView::chunkGroup() const {
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
        ViewportArea.moveTop(y);
        UpdateActiveChunkViews();
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
        UpdateActiveChunkViews();
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
//    if(parentItem() != NULL) {
//        return parentItem()->boundingRect();
//    } else {
//        return QRectF(0, 0, width(), height());
//    }
//}

/**
  \brief Updates active chunks

  This will check what chunks should be shown, and which chunk should be hidden
  The hidden chunks will be deleted.  New views may be created, if nessary
  */
void cwSurveyChunkGroupView::UpdateActiveChunkViews() {
    QPair<int, int> range = VisableRange();
    if(range.first < 0 || range.first >= ChunkViews.size()) { return; }
    if(range.second < 0 || range.second >= ChunkViews.size()) { return; }

    float oldContentWidth = childrenBoundingRect().width();

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
        CreateChunkView(i);
    }

    //Update the positions off all the visible elements
    //and the navigation
    for(int i = range.first; i <= range.second; i++) {
        ChunkViews[i]->setY(ChunkBoundingRects[i].top());

        //Update the navigation for the chunk
        cwSurveyChunkView* aboveChunk = NULL;
        cwSurveyChunkView* belowChunk = NULL;

        int aboveIndex = i - 1;
        int belowIndex = i + 1;

        if(aboveIndex >= 0 && aboveIndex < ChunkViews.size()) {
            aboveChunk = ChunkViews[aboveIndex];
        }

        if(belowIndex >= 0 && belowIndex < ChunkViews.size()) {
            belowChunk = ChunkViews[belowIndex];
        }

        ChunkViews[i]->SetNavigation(aboveChunk, belowChunk);
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
    QList<QRectF>::iterator firstIter = qLowerBound(ChunkBoundingRects.begin(), ChunkBoundingRects.end(), QRectF(ViewportArea.topLeft(), QSize(1, 1)), lessThanChunkRect);
    QList<QRectF>::iterator lastIter = qLowerBound(ChunkBoundingRects.begin(), ChunkBoundingRects.end(), QRectF(ViewportArea.bottomLeft(), QSize(1, 1)), lessThanChunkRect);


    //qDebug() << "First: " << firstIter - ChunkBoundingRects.begin() << " Second: " << lastIter - ChunkBoundingRects.begin();
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
    if(ChunkViews[index] == NULL) {
        QModelIndex currentIndex = Trip->index(index);
        cwSurveyChunk* chunk = qobject_cast<cwSurveyChunk*>(Trip->data(currentIndex, cwTrip::ChunkRole).value<QObject*>());
        if(chunk == NULL) { return; }

        //Create a chunkView object
        cwSurveyChunkView* chunkView = new cwSurveyChunkView(this);
        QDeclarativeContext* context = QDeclarativeEngine::contextForObject(this);
        QDeclarativeEngine::setContextForObject(chunkView, context);
        chunkView->SetQMLComponents(ChunkQMLComponents);

        //Set the model for the view
        chunkView->setModel(chunk);

        //Make sure we update when the chunkHasChanged
        connect(chunkView, SIGNAL(heightChanged()), SLOT(UpdateChunkHeight()));

        //Updates the model when items are split
        connect(chunkView, SIGNAL(createdNewChunk(cwSurveyChunk*)), SLOT(HandleSplitChunk(cwSurveyChunk*)));

        //Make sure the chunkView keeps the visiblity for items when focused
        connect(chunkView, SIGNAL(ensureVisibleChanged(QRectF)), SLOT(SetEnsureVisibleRect(QRectF)));

        //Update the position of the view at index i

        //UpdatePosition(index);

        ChunkViews[index] = chunkView;
        //qDebug() << "Chunk Bounding box:" << index << chunkView->boundingRect();
    }
}

/**
  \brief Deletes the chunk from the view

  This doesn't remove a chunk view at the index, it simply deletes the view and
  sets the index to NULL
  */
void cwSurveyChunkGroupView::DeleteChunkView(int index) {
    if(ChunkViews[index] != NULL) {
        ChunkViews[index]->deleteLater();
        ChunkViews[index] = NULL;
    }
}

/**
  \brief Sets the focus for a view at index

  This will cause a view to become focused
  */
void cwSurveyChunkGroupView::SetFocus(int index) {
    if(index < 0 || index >= ChunkViews.size()) { return; }
    CreateChunkView(index); //Make sure the chunk exists;
    cwSurveyChunkView* surveyChunk = ChunkViews.at(index);
    surveyChunk->setFocus(true);
}

/**
  \brief Insert rows
  */
void cwSurveyChunkGroupView::InsertRows(const QModelIndex &parent, int start, int end) {
    //qDebug() << "Insert Rows:" << parent << start << end;
    if(parent != QModelIndex()) { return; }
    AddChunks(start, end);
}

/**
  Adds new chunks to the view from beginIndex to endIndex
  */
void cwSurveyChunkGroupView::AddChunks(int beginIndex, int endIndex) {
    if(Trip == NULL) { return; }
    ///qDebug() << "Adding chunks " << beginIndex << endIndex;

    for(int i = beginIndex; i <= endIndex; i++) {
        QModelIndex currentIndex = Trip->index(i);
        cwSurveyChunk* chunk = qobject_cast<cwSurveyChunk*>(Trip->data(currentIndex, cwTrip::ChunkRole).value<QObject*>());
        if(chunk == NULL) { continue; }

        ChunkViews.insert(i, NULL);
        ChunkBoundingRects.insert(i, QRectF());
    }

    UpdateContentArea(beginIndex, endIndex);
    UpdateActiveChunkViews();

    //If only one chunk is added set the focus on it
    if(beginIndex == endIndex) {
        SetFocus(beginIndex);
    }
}

/**
  Updates the chunk's height

  This is used with a signalling cwSurveyChunkView that thier height has changed
  */
void cwSurveyChunkGroupView::UpdateChunkHeight() {
    cwSurveyChunkView* chunkView = qobject_cast<cwSurveyChunkView*>(sender());
    if(chunkView != NULL) {
        int index = ChunkViews.indexOf(chunkView);
        UpdateContentArea(index, index);
        UpdateActiveChunkViews();
    }
}

/**
  \brief Ensure that rect is visible

  This emit a signal such that, qml flickable area will scroll such that the rect
  will be in the visible area
  */
void cwSurveyChunkGroupView::SetEnsureVisibleRect(QRectF rect) {
    EnsureVisibleArea = rect;
    emit ensureVisibleRectChanged();
}

/**
  \brief This adds split chunk into the model

  newChunk is the new chunk that needs to be added to the model.  This happens when
  the chunk has been split into two chunks, by the user
  */
void cwSurveyChunkGroupView::HandleSplitChunk(cwSurveyChunk *newChunk) {

    cwSurveyChunkView* chunkView = qobject_cast<cwSurveyChunkView*>(sender());
    if(chunkView == NULL) { return; }

    //Find the chunk
    int firstIndex = VisableRange().first; //Only the visible range is valid
    int insertIndex = ChunkViews.indexOf(chunkView, firstIndex) + 1;

    Trip->insertRow(insertIndex, newChunk);
}

/**
  Updates the surveychunkview's position at index
  */
void cwSurveyChunkGroupView::UpdatePosition(int index) {

    int previousIndex = index - 1;
    if(previousIndex < 0) {
        ChunkViews[index]->setPos(0, 0);
        return;
    }

    cwSurveyChunkView* previousView = ChunkViews[previousIndex];
    //const float buffer = 10.0;
    float x = 0.0;
    float y = mapRectFromItem(previousView, previousView->boundingRect()).bottom(); //height(); // + buffer;

    cwSurveyChunkView* view = ChunkViews[index];
    view->setPos(x, y);
}

/**
  \brief Updates the ChunkBoundingRects and ChunkYPosition between and include the index
  of beginIndex and endIndex

  Will update the interal positions of those chunks, this allows the caching of elements
  */
void cwSurveyChunkGroupView::UpdateContentArea(int beginIndex, int endIndex) {
    //Make sure beginIndex and endIndex are good
    if(beginIndex < 0 || beginIndex >= Trip->rowCount()) { return; }
    if(endIndex < 0 || endIndex >= Trip->rowCount()) { return; }
    if(beginIndex > endIndex) { return; }

    //Calc the yOffset
    float yOffset;
    int beforeIndex = beginIndex - 1;
    if(beforeIndex >= 0) {
        QRectF beforeRect = ChunkBoundingRects[beforeIndex];
        yOffset = beforeRect.bottom();
    } else {
        yOffset = 0;
    }

    for(int i = beginIndex; i <= endIndex; i++) {
        QModelIndex currentIndex = Trip->index(i);
        cwSurveyChunk* chunk = qobject_cast<cwSurveyChunk*>(Trip->data(currentIndex, cwTrip::ChunkRole).value<QObject*>());
        if(chunk == NULL) { continue; }

        float height = cwSurveyChunkView::heightHint(chunk->StationCount());
        ChunkBoundingRects[i].setHeight(height);
        ChunkBoundingRects[i].setWidth(1);
        ChunkBoundingRects[i].moveTop(yOffset);
        yOffset = ChunkBoundingRects[i].bottom();
    }

    //Update the reminding chunks below
    for(int i = endIndex + 1; i < ChunkBoundingRects.size(); i++) {
        ChunkBoundingRects[i].moveTop(yOffset);
        yOffset = ChunkBoundingRects[i].bottom();
    }

    //Update the positions off all the visible elements
    for(int i = beginIndex; i < ChunkViews.size(); i++) {
        if(ChunkViews[i] != NULL) {
            ChunkViews[i]->setY(ChunkBoundingRects[i].top());
        }
    }

    emit contentHeightChanged();
}

/**
  \brief Get's the content's height of the view

  This is the height if all the elements where displayed at once
  */
float cwSurveyChunkGroupView::contentHeight() const {
    if(ChunkBoundingRects.isEmpty()) { return 0.0; }
    return ChunkBoundingRects.last().bottom() - ChunkBoundingRects.first().top() + 1;
}

float cwSurveyChunkGroupView::contentWidth() const {
    if(ChunkBoundingRects.isEmpty()) { return 0.0; }
//    qDebug() << "Children width: " << childrenBoundingRect().width();
    return childrenBoundingRect().width();
}


