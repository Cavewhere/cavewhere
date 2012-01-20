//Our includes
#include "cwSurveyChunkView.h"
#include "cwSurveyChunk.h"
#include "cwStationReference.h"
#include "cwShot.h"
#include "cwSurveyChunkViewComponents.h"
#include "cwStationValidator.h"
#include "cwDistanceValidator.h"

//Qt includes
#include <QMetaObject>
#include <QDeclarativeEngine>
#include <QDeclarativeContext>
#include <QVariant>
#include <QMenu>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <QCursor>

cwSurveyChunkView::cwSurveyChunkView(QDeclarativeItem* parent) :
    QDeclarativeItem(parent),
    SurveyChunk(NULL),
    QMLComponents(NULL),
    FocusedItem(NULL),
    ChunkBelow(NULL),
    ChunkAbove(NULL)

{
    connect(this, SIGNAL(focusChanged(bool)), SLOT(setFocusForFirstStation(bool)));

}

cwSurveyChunkView::~cwSurveyChunkView() {

}

/**
  \brief Get's the elements height
  */
float cwSurveyChunkView::elementHeight() { return 40; }

/**
  \brief Estimates the height of the view with numberElements
  */
float cwSurveyChunkView::heightHint(int numberElements) {
    const int buffer = 10;
    return (numberElements + 1) * (elementHeight() + 2) - 2 + buffer; //Plus 1 for the title
}

/**
  \brief Sets the navigation for this object

  If the user use the keyboard to move the focus, below is the object below this object.
  This will update the navigation for the object below this
  */
void cwSurveyChunkView::setNavigationBelow(const cwSurveyChunkView* below) {
    ChunkBelow = below;

    if(StationRows.empty()) { return; }

    //Update first station navigation
    int lastIndex = StationRows.size() - 1;
    updateStationTabNavigation(lastIndex);
    updateStationArrowNavigation(lastIndex);

    lastIndex = ShotRows.size() - 1;
    updateShotArrowNavigaton(lastIndex);
}

/**
  \brief Sets the navigation for this object

  If the user use the keyboard to move the focus, above is the object above this
  object.  This will update the navigation for the first last object in this view
  */
void cwSurveyChunkView::setNavigationAbove(const cwSurveyChunkView* above) {
    ChunkAbove = above;

    if(StationRows.empty()) { return; }

    updateStationTabNavigation(0);
    updateStationArrowNavigation(0);
    updateShotArrowNavigaton(0);
}

/**
  \brief Convenience method

  Equivalent to SetNavigationBelow and SetNavigationAbove
  */
void cwSurveyChunkView::setNavigation(const cwSurveyChunkView* above, const cwSurveyChunkView* below) {
    setNavigationAbove(above);
    setNavigationBelow(below);
}

void cwSurveyChunkView::setQMLComponents(cwSurveyChunkViewComponents* components) {
    QMLComponents = components;
}

/**
  \brief This update the survey chunk's data
  */
void cwSurveyChunkView::updateData(cwSurveyChunk::DataRole role, int index, const QVariant& data) {
    switch (role) {
    case cwSurveyChunk::StationNameRole:
    case cwSurveyChunk::StationLeftRole:
    case cwSurveyChunk::StationRightRole:
    case cwSurveyChunk::StationUpRole:
    case cwSurveyChunk::StationDownRole:
        updateStationData(role, index, data);
        break;
    case cwSurveyChunk::ShotDistanceRole:
    case cwSurveyChunk::ShotCompassRole:
    case cwSurveyChunk::ShotBackCompassRole:
    case cwSurveyChunk::ShotClinoRole:
    case cwSurveyChunk::ShotBackClinoRole:
        updateShotData(role, index, data);
        break;
    }
}

/**
  \brief Get's the model from the view
  */
cwSurveyChunk* cwSurveyChunkView::model() {
    return SurveyChunk;
}

/**
  \brief Set's the model for the view
  */
void cwSurveyChunkView::setModel(cwSurveyChunk* chunk) {
    if(QMLComponents == NULL) {
        qDebug() << "QML components need to be set before setting the model";
    }

    if(SurveyChunk != chunk) {
        if(SurveyChunk != NULL) {
            disconnect(SurveyChunk, NULL, this, NULL);
        }

        SurveyChunk = chunk;

        //SetupDelegates();

        clear();
        addStations(0, SurveyChunk->stationCount() - 1);
        addShots(0, SurveyChunk->shotCount() - 1);

        connect(SurveyChunk, SIGNAL(shotsAdded(int,int)), SLOT(addShots(int,int)));
        connect(SurveyChunk, SIGNAL(stationsAdded(int,int)), SLOT(addStations(int,int)));
        connect(SurveyChunk, SIGNAL(shotsRemoved(int,int)), SLOT(removeShots(int,int)));
        connect(SurveyChunk, SIGNAL(stationsRemoved(int,int)), SLOT(removeStations(int,int)));
        connect(SurveyChunk, SIGNAL(dataChanged(cwSurveyChunk::DataRole,int,QVariant)), SLOT(updateData(cwSurveyChunk::DataRole,int,QVariant)));

        emit modelChanged();
    }
}

QRectF cwSurveyChunkView::boundingRect() const {
    return childrenBoundingRect();
}

/**
  \brief Re-initializes the view
  */
void cwSurveyChunkView::clear() {
    foreach(QGraphicsItem* item, childItems()) {
        delete item;
    }

    StationRows.clear();
    ShotRows.clear();
    StationToIndex.clear();

    createTitlebar();
}

/**
  \brief User right clicks on station
  */
void cwSurveyChunkView::rightClickOnStation(int index) {

    QMenu menu;

    QAction* insertAbove = menu.addAction(QIcon(":icons/AddArrowUp.png"), "Insert Above");
    QAction* insertBelow = menu.addAction(QIcon(":icons/AddArrowDown.png"),"Insert Below");
    menu.addSeparator();
    QAction* removeAbove = menu.addAction(QIcon(":icons/RemoveArrowUp.png"),"Remove Above");
    QAction* removeBelow = menu.addAction(QIcon(":icons/RemoveArrowDown.png"),"Remove Below");

    removeAbove->setEnabled(SurveyChunk->canRemoveStation(index, cwSurveyChunk::Above));
    removeBelow->setEnabled(SurveyChunk->canRemoveStation(index, cwSurveyChunk::Below));

    QAction* selected = menu.exec(QCursor::pos());
    if(selected == insertAbove) {
        SurveyChunk->insertStation(index, cwSurveyChunk::Above);
    } else if(selected == insertBelow) {
        SurveyChunk->insertStation(index, cwSurveyChunk::Below);
    } else if(selected == removeAbove) {
        SurveyChunk->removeStation(index, cwSurveyChunk::Above);
    } else if(selected == removeBelow) {
        SurveyChunk->removeStation(index, cwSurveyChunk::Below);
    }
}

/**
  \brief User right clicks on station
  */
void cwSurveyChunkView::rightClickOnShot(int index) {
    QMenu menu;

    QAction* insertAbove = menu.addAction(QIcon(":icons/AddArrowUp.png"), "Insert Above");
    QAction* insertBelow = menu.addAction(QIcon(":icons/AddArrowDown.png"),"Insert Below");
    menu.addSeparator();
    QAction* removeAbove = menu.addAction(QIcon(":icons/RemoveArrowUp.png"),"Remove Above");
    QAction* removeBelow = menu.addAction(QIcon(":icons/RemoveArrowDown.png"),"Remove Below");

    removeAbove->setEnabled(SurveyChunk->canRemoveShot(index, cwSurveyChunk::Above));
    removeBelow->setEnabled(SurveyChunk->canRemoveShot(index, cwSurveyChunk::Below));

    QAction* selected = menu.exec(QCursor::pos());
    if(selected == insertAbove) {
        SurveyChunk->insertShot(index, cwSurveyChunk::Above);
    } else if(selected == insertBelow) {
        SurveyChunk->insertShot(index, cwSurveyChunk::Below);
    } else if(selected == removeAbove) {
        SurveyChunk->removeShot(index, cwSurveyChunk::Above);
    } else if(selected == removeBelow) {
        SurveyChunk->removeShot(index, cwSurveyChunk::Below);
    }
}


/**
  \brief Splits the chunk into two pieces on a station

  \param int - The station index where the station will be
  split

  This will emit a InsertNewChunk()
  */
void cwSurveyChunkView::splitOnStation(int index) {
    qDebug() << "cwSurveyChunkView: split on station" << index;

    //Make sure the index is good
    if(index < 0 || index >= SurveyChunk->stationCount()) {
        return; //Can't split on a bad index
    }

    cwSurveyChunk* newChunk = SurveyChunk->splitAtStation(index);
    emit createdNewChunk(newChunk);
}

/**
  \brief Splits the chunk into two pieces on a station

  \param int - The shot index where the chunk will be split
*/
void cwSurveyChunkView::splitOnShot(int /*index*/) {



}


/**
  \brief Add stations to the view

  This inserts new stations found in the model at and between beginIndex and endIndex.
  */
void cwSurveyChunkView::addStations(int beginIndex, int endIndex) {
    //Make sure the model has data
    if(SurveyChunk->stationCount() == 0) { return; }

    //Alert the scene graph that we're changing
    prepareGeometryChange();

    //Disconnect stations that were watched
    disconnect(this, SLOT(stationValueHasChanged()));

    //Make sure these are good indexes, else clamp
    beginIndex = qMax(0, beginIndex);
    endIndex = qMin(SurveyChunk->stationCount() - 1, endIndex);

    for(int i = beginIndex; i <= endIndex; i++) {
        //Create the row
        StationRow row(this, i);

        //Insert the row
        StationRows.insert(i, row);

        //Update the row's data
        updateStationRowData(i);

        //Position the row in the correct place
        positionStationRow(row, i);

        //Hock up the signals and slots with the models data
        cwStationReference station = SurveyChunk->station(i);
        StationToIndex[station] = i;

        //Queue the index for navigation update
        StationNavigationQueue.append(i);
    }

    //Add previous station and next station to the navigation queue
    StationNavigationQueue.append(beginIndex - 1);
    StationNavigationQueue.append(endIndex + 1);

    //Update the navigation
    updateNavigation();

    //Connect the last station's in the view
    updateLastRowBehaviour();

    //Update the positions for all rows after endIndex
    updatePositionsAfterIndex(endIndex + 1);

    //Update the id's on effected rows
    updateIndexes(endIndex + 1);

    updateDimensions();

    //qDebug() << boundingRect();
}

/**
  \brief Adds shots to the view

  This inserts new shots found in the model at and between beginIndex and endIndex
  */
void cwSurveyChunkView::addShots(int beginIndex, int endIndex) {
    //Make sure the model has data
    if(SurveyChunk->shotCount() == 0) { return; }

    //Alert the scene graph that we're changing
    prepareGeometryChange();

    //Make sure thes are good indexes, else clamp
    beginIndex = qMax(0, beginIndex);
    endIndex = qMin(SurveyChunk->shotCount() - 1, endIndex);

    for(int i = beginIndex; i <= endIndex; i++) {
        //Create the row
        ShotRow row(this, i);

        //Insert the row
        ShotRows.insert(i, row);

        //Update the row with data
        updateShotRowData(i);

        //Position the row in the correct place
        positionShotRow(row, i);

        //Hock up the signals and slots with the model's data
        cwShot* shot = SurveyChunk->shot(i);
        connectShot(shot, row);

        //Queue the index for navigation update
        ShotNavigationQueue.append(i);
    }

    //Add previous station and next station to the navigation queue
    ShotNavigationQueue.append(beginIndex - 1);
    ShotNavigationQueue.append(endIndex + 1);
    updateNavigation();

    //Update the positions for all rows after endIndex
    updatePositionsAfterIndex(endIndex + 1);

    //Update the id's on effected rows
    updateIndexes(endIndex + 1);

    updateDimensions();

}

/**
  \brief Removes stations from the view
  */
void cwSurveyChunkView::removeStations(int beginIndex, int endIndex) {

    //Make sure the index are good
    beginIndex = qMax(0, beginIndex);
    endIndex = qMin(StationRows.size() - 1, endIndex);

    for(int i = endIndex; i >= beginIndex; i--) {
        StationRow row = getStationRow(i);

        foreach(QDeclarativeItem* item, row.items()) {
            if(item) {
                item->deleteLater();
            }
        }

        //Remove the station from the look up
        cwStationReference station = SurveyChunk->station(i);
        StationToIndex.remove(station);

        //Remove the row from the station rows
        StationRows.removeAt(i);
    }

    //Update the navigation for the remaining rows
    StationNavigationQueue.append(beginIndex - 1);
    StationNavigationQueue.append(beginIndex);
    StationNavigationQueue.append(beginIndex + 1);
    updateNavigation();

    //Update the positions for all rows after endIndex
    updatePositionsAfterIndex(beginIndex);

    //Update the id's on effected rows
    updateIndexes(beginIndex);

    updateDimensions();
}

/**
  \brief Remove shots from the view
  */
void cwSurveyChunkView::removeShots(int beginIndex, int endIndex) {
    //Make sure the index are good
    beginIndex = qMax(0, beginIndex);
    endIndex = qMin(ShotRows.size() - 1, endIndex);

    for(int i = endIndex; i >= beginIndex; i--) {
        ShotRow row = getShotRow(i);

        foreach(QDeclarativeItem* item, row.items()) {
            if(item) {
                item->deleteLater();
            }
        }

        //Remove the row from the station rows
        ShotRows.removeAt(i);
    }

    //Update the navigation for the remaining rows
    ShotNavigationQueue.append(beginIndex - 1);
    ShotNavigationQueue.append(beginIndex);
    ShotNavigationQueue.append(beginIndex + 1);

    updateNavigation();

    //Update the positions for all rows after endIndex
    updatePositionsAfterIndex(beginIndex);

    //Update the id's on effected rows
    updateIndexes(beginIndex);

    updateDimensions();
}


/**
  \brief Creates the title bar for the view
  */
void cwSurveyChunkView::createTitlebar() {

    QDeclarativeComponent* titleDelegate = QMLComponents->titleDelegate();
    StationTitle = qobject_cast<QDeclarativeItem*>(titleDelegate->create());
    DistanceTitle = qobject_cast<QDeclarativeItem*>(titleDelegate->create());
    AzimuthTitle = qobject_cast<QDeclarativeItem*>(titleDelegate->create());
    ClinoTitle = qobject_cast<QDeclarativeItem*>(titleDelegate->create());
    LeftTitle = qobject_cast<QDeclarativeItem*>(titleDelegate->create());
    RightTitle = qobject_cast<QDeclarativeItem*>(titleDelegate->create());
    UpTitle = qobject_cast<QDeclarativeItem*>(titleDelegate->create());
    DownTitle = qobject_cast<QDeclarativeItem*>(titleDelegate->create());

    StationTitle->setParentItem(this);
    DistanceTitle->setParentItem(this);
    AzimuthTitle->setParentItem(this);
    ClinoTitle->setParentItem(this);
    LeftTitle->setParentItem(this);
    RightTitle->setParentItem(this);
    UpTitle->setParentItem(this);
    DownTitle->setParentItem(this);

    StationTitle->setProperty("text", "Station");
    DistanceTitle->setProperty("text", "Distance");
    AzimuthTitle->setProperty("text", "Azimuth");
    ClinoTitle->setProperty("text", "Vertical\nAngle");
    LeftTitle->setProperty("text", "L");
    RightTitle->setProperty("text", "R");
    UpTitle->setProperty("text", "U");
    DownTitle->setProperty("text", "D");

    StationTitle->setProperty("height", elementHeight());
    DistanceTitle->setProperty("height", elementHeight());
    AzimuthTitle->setProperty("height", elementHeight());
    ClinoTitle->setProperty("height", elementHeight());
    LeftTitle->setProperty("height", elementHeight());
    RightTitle->setProperty("height", elementHeight());
    UpTitle->setProperty("height", elementHeight());
    DownTitle->setProperty("height", elementHeight());

    StationTitle->setPos(0, 0);
    DistanceTitle->setX(mapRectFromItem(StationTitle, StationTitle->boundingRect()).right() - 1);
    DistanceTitle->setY(mapRectFromItem(StationTitle, StationTitle->boundingRect()).center().y() + 1);
    AzimuthTitle->setX(mapRectFromItem(DistanceTitle, DistanceTitle->boundingRect()).right() - 1);
    AzimuthTitle->setY(mapRectFromItem(DistanceTitle, DistanceTitle->boundingRect()).top() + 1);
    ClinoTitle->setX(mapRectFromItem(AzimuthTitle, AzimuthTitle->boundingRect()).right() - 1);
    ClinoTitle->setY(mapRectFromItem(AzimuthTitle, AzimuthTitle->boundingRect()).top() + 1);
    LeftTitle->setX(mapRectFromItem(ClinoTitle, ClinoTitle->boundingRect()).right() - 1);
    LeftTitle->setY(mapRectFromItem(StationTitle, StationTitle->boundingRect()).top() + 1);
    RightTitle->setX(mapRectFromItem(LeftTitle, LeftTitle->boundingRect()).right() - 1);
    RightTitle->setY(mapRectFromItem(StationTitle, StationTitle->boundingRect()).top() + 1);
    UpTitle->setX(mapRectFromItem(RightTitle, RightTitle->boundingRect()).right() - 1);
    UpTitle->setY(mapRectFromItem(StationTitle, StationTitle->boundingRect()).top() + 1);
    DownTitle->setX(mapRectFromItem(UpTitle, UpTitle->boundingRect()).right() - 1);
    DownTitle->setY(mapRectFromItem(StationTitle, StationTitle->boundingRect()).top() + 1);

}

/**
  \brief Constructor for a station row
  */
cwSurveyChunkView::StationRow::StationRow() : Row(-1, NumberItems)
{

}

/**
  \brief Constructor to the StationRow
  */
cwSurveyChunkView::StationRow::StationRow(cwSurveyChunkView* view, int rowIndex) : Row(rowIndex, NumberItems) {
    cwSurveyChunkViewComponents* components = view->QMLComponents;
    QDeclarativeContext* context = QDeclarativeEngine::contextForObject(view);
    Items[StationName] = setupItem(components->stationDelegate(),
                                   context,
                                   cwSurveyChunk::StationNameRole,
                                   components->stationValidator());

    Items[Left]= setupItem(components->leftDelegate(),
                           context,
                           cwSurveyChunk::StationLeftRole,
                           components->lrudValidator());

    Items[Right] = setupItem(components->rightDelegate(),
                             context,
                             cwSurveyChunk::StationRightRole,
                             components->lrudValidator());

    Items[Up] = setupItem(components->upDelegate(),
                          context,
                          cwSurveyChunk::StationUpRole,
                          components->lrudValidator());

    Items[Down] = setupItem(components->downDelegate(),
                            context,
                           cwSurveyChunk::StationDownRole,
                           components->lrudValidator());

    foreach(QDeclarativeItem* item, items()) {
        item->setProperty("rowIndex", rowIndex);
        item->setProperty("surveyChunk", QVariant::fromValue(view->model()));
        item->setParentItem(view);
        item->setParent(view);
    }
}

cwSurveyChunkView::Row::Row(int rowIndex, int numberOfItems) {
    RowIndex = rowIndex;

    Items.reserve(numberOfItems);
    Items.resize(numberOfItems);
    for(int i = 0; i < Items.size(); i++) {
        Items[i] = NULL;
    }
}

/**
  \brief Sets up a row declarative item
  */
QDeclarativeItem* cwSurveyChunkView::Row::setupItem(QDeclarativeComponent* component,
                                                    QDeclarativeContext* context,
                                                    cwSurveyChunk::DataRole role,
                                                    QValidator *validator) {
    if(component->isError()) {
        qWarning() << component->errorString();
        return NULL;
    }

    QDeclarativeItem* item = qobject_cast<QDeclarativeItem*>(component->create(context));
    item->setProperty("dataRole", role);
    item->setProperty("dataValidator", QVariant::fromValue(static_cast<QObject*>(validator)));
    return item;
}

/**
  \brief Constructor for a shot row
  */
cwSurveyChunkView::ShotRow::ShotRow() : Row(-1, NumberItems)
{

}

/**
  \brief Constructor to the ShotRow
  */
cwSurveyChunkView::ShotRow::ShotRow(cwSurveyChunkView *view, int rowIndex) : Row(rowIndex, NumberItems) {
    cwSurveyChunkViewComponents* components = view->QMLComponents;
    QDeclarativeContext* context = QDeclarativeEngine::contextForObject(view);
    Items[Distance] = setupItem(components->distanceDelegate(),
                                context,
                                cwSurveyChunk::ShotDistanceRole,
                                components->distanceValidator());

    Items[FrontCompass] = setupItem(components->frontCompassDelegate(),
                                    context,
                                     cwSurveyChunk::ShotCompassRole,
                                     components->compassValidator());

    Items[BackCompass] = setupItem(components->backCompassDelegate(),
                                   context,
                                   cwSurveyChunk::ShotBackCompassRole,
                                   components->compassValidator());

    Items[FrontClino]  = setupItem(components->frontClinoDelegate(),
                                   context,
                                   cwSurveyChunk::ShotClinoRole,
                                   components->clinoValidator());

    Items[BackClino] = setupItem(components->backClinoDelegate(),
                                 context,
                                 cwSurveyChunk::ShotBackClinoRole,
                                 components->clinoValidator());

    foreach(QDeclarativeItem* item, items()) {
        item->setProperty("rowIndex", rowIndex);
        item->setProperty("surveyChunk", QVariant::fromValue(view->model()));
        item->setParentItem(view);
        item->setParent(view);
    }
}

/**
  \brief This positions the station row

  Will position the row based no the model's index of the row.
  All the elements in station row will be position correctly
  */
void cwSurveyChunkView::positionStationRow(StationRow row, int index) {
    positionElement(row.stationName(), StationTitle, index);
    positionElement(row.left(), LeftTitle, index);
    positionElement(row.right(), RightTitle, index);
    positionElement(row.up(), UpTitle, index);
    positionElement(row.down(), DownTitle, index);
}

/**
  \brief Helper to the PositionStationRow
  */
void cwSurveyChunkView::positionElement(QDeclarativeItem* item, const QDeclarativeItem* titleItem, int index, int yOffset, QSizeF size) {
    QRectF titleRect = mapRectFromItem(titleItem, titleItem->boundingRect());
    size = !size.isValid() ? titleRect.size() + QSizeF(-2, 0) : size;
    float y = titleRect.bottom() + titleRect.height() * index;
    QPointF position(titleRect.left() + 1, yOffset + y -1);
    //item->setPos(position - QPointF(0.0, size.height()));
    if(item->pos() == QPointF(0.0, 0.0)) {
        //Not position yet
        item->setOpacity(0.0);
        item->setPos(position);
        item->setProperty("opacity", 1.0);
    } else {
        //Animate the change in position
        //item->setProperty("x", position.x());
        item->setX(position.x());
        item->setProperty("y", position.y());
    }

    item->setSize(size);
}

/**
  \brief Hooks up the model to the qml, for the station row
  */
void cwSurveyChunkView::connectStation(cwStationReference* /*station*/, StationRow /*row*/) {
    //    QVariant stationObject = QVariant::fromValue(static_cast<QObject*>(station));

    //    QVector<QDeclarativeItem*> items = row.items();
    //    foreach(QDeclarativeItem* item, items) {
    //        item->setProperty("dataObject", stationObject);
    //        item->setProperty("rowIndex", row.rowIndex());
    //        connect(item, SIGNAL(rightClicked(int)), SLOT(RightClickOnStation(int)));
    //        connect(item, SIGNAL(splitOn(int)), SLOT(SplitOnStation(int)));
    //        connect(item, SIGNAL(focusChanged(bool)), SLOT(SetChildActiveFocus(bool)));
    //    }
}

/**
  \brief Positions the shot row in the object
  */
void cwSurveyChunkView::positionShotRow(ShotRow row, int index) {
    positionElement(row.distance(), DistanceTitle, index);

    float halfAzimuthHeight = AzimuthTitle->height() / 2.0 + 1.0;
    float halfClinoHeight = ClinoTitle->height() / 2.0 + 1.0;
    positionElement(row.frontCompass(), AzimuthTitle, index, 0, QSize(AzimuthTitle->width(), halfAzimuthHeight));
    positionElement(row.backCompass(), AzimuthTitle, index, halfAzimuthHeight, QSize(AzimuthTitle->width(), halfAzimuthHeight));
    positionElement(row.frontClino(), ClinoTitle, index, 0, QSize(ClinoTitle->width(), halfClinoHeight));
    positionElement(row.backClino(), ClinoTitle, index, halfClinoHeight, QSize(ClinoTitle->width(), halfClinoHeight));
}

/**
  \brief Hooks up the model to the qml, for the shot row
  */
void cwSurveyChunkView::connectShot(cwShot* /*shot*/, ShotRow /*row*/) {
    //    QVariant shotObject = QVariant::fromValue(static_cast<QObject*>(shot));

    //    QVector<QDeclarativeItem*> items = row.items();
    //    foreach(QDeclarativeItem* item, items) {
    //        item->setProperty("dataObject", shotObject);
    //        item->setProperty("rowIndex", row.rowIndex());
    //        connect(item, SIGNAL(rightClicked(int)), SLOT(RightClickOnShot(int)));
    //        connect(item, SIGNAL(splitOn(int)), SLOT(SplitOnShot(int)));
    //        connect(item, SIGNAL(focusChanged(bool)), SLOT(SetChildActiveFocus(bool)));
    //    }
}

//void cwSurveyChunkView::StationFocusChanged(bool focus) {
//    qDebug() << "Focus changed:" << focus;
//    if(!Model || Model->StationCount() == 0) { return; }
//    QDeclarativeItem* item = qobject_cast<QDeclarativeItem*>(sender());
//    if(item == NULL) { return; }
//    cwStation* station = qobject_cast<cwStation*>(item->property("dataObject").value<QObject*>());
//    if(station == NULL) { return; }

//    //If this is the last station
//    int lastIndex = Model->StationCount() - 1;
//    if(station == Model->Station(lastIndex)) {
//        Model->AppendNewShot();
//    }


//}

/**
  \brief Called when this item get's focus
  */
void cwSurveyChunkView::setFocusForFirstStation(bool focus) {
    //If this view has focus
    if(focus && !StationRows.isEmpty()) {
        StationRow row = StationRows.first();
        row.stationName()->setFocus(true); //Focus the first station
    }
}

/**
  \brief Called when the children focus has changed
  */
void cwSurveyChunkView::setChildActiveFocus(bool focus) {
    if(sender() == FocusedItem && !focus) {
        //Losted focus
        FocusedItem = NULL;
    } else if (sender() != FocusedItem) {
        //Gained focus
        FocusedItem = qobject_cast<QDeclarativeItem*>(sender());
    }

    if(FocusedItem != NULL) {
        QRectF localRect = FocusedItem->mapRectToParent(FocusedItem->childrenBoundingRect());
        QRectF parentRect = mapRectToParent(localRect);
        ensureVisibleChanged(parentRect);
    }
}


/**
  \brief Called when the station's value has changed
  */
void cwSurveyChunkView::stationValueHasChanged() {
    int stationCount = SurveyChunk->stationCount();
    if(stationCount < 2) { return; }

    StationRow lastStation = getStationRow(stationCount - 1);
    StationRow secondLastStation = getStationRow(stationCount - 2);

    if(lastStation.stationName() == NULL || secondLastStation.stationName() == NULL) { return; }

    QString lastStationName = lastStation.stationName()->property("dataValue").toString();
    QString secondLastStationName = secondLastStation.stationName()->property("dataValue").toString();


    if(lastStationName.isEmpty() && secondLastStationName.isEmpty()) {
        ShotRow lastShotRow = getShotRow(SurveyChunk->shotCount() - 1);

        //Remove the lastStation
        if(lastStation.left()->property("dataValue").toString().isEmpty() &&
                lastStation.right()->property("dataValue").toString().isEmpty() &&
                lastStation.up()->property("dataValue").toString().isEmpty() &&
                lastStation.down()->property("dataValue").toString().isEmpty() &&
                lastShotRow.distance()->property("dataValue").toString().isEmpty() &&
                lastShotRow.frontCompass()->property("dataValue").toString().isEmpty() &&
                lastShotRow.backCompass()->property("dataValue").toString().isEmpty() &&
                lastShotRow.frontClino()->property("dataValue").toString().isEmpty() &&
                lastShotRow.backClino()->property("dataValue").toString().isEmpty()) {

            SurveyChunk->removeStation(SurveyChunk->stationCount() - 1, cwSurveyChunk::Above);
        }


    } else if(!lastStationName.isEmpty()) {
        //Add station to the model
        SurveyChunk->appendNewShot();
    }
}



/**
  \brief Updates the navigation

  Updates the navigation for pending elements in the StationNavigationQueue and
  the ShotNavigationQueue and there neighbors
  */
void cwSurveyChunkView::updateNavigation() {

    //Valid that we can update navigation
    if(StationRows.empty() || ShotRows.empty()) { return; }
    if(StationRows.size() - 1 != ShotRows.size()) { return; }

    foreach(int index, StationNavigationQueue) {
        updateStationTabNavigation(index);
        updateStationArrowNavigation(index);
    }

    foreach(int index, ShotNavigationQueue) {
        updateShotTabNavigation(index);
        updateShotArrowNavigaton(index);
    }

    StationNavigationQueue.clear();
    ShotNavigationQueue.clear();
}

/**
  \brief Helper function to Update Navigation

  Update the tab order for the staton row at index
  */
void cwSurveyChunkView::updateStationTabNavigation(int index) {

    StationRow previousRow = getNavigationStationRow(index - 1);
    StationRow currentRow = getNavigationStationRow(index);
    StationRow nextRow = getNavigationStationRow(index + 1);

    ShotRow previousShotRow = getShotRow(index - 1);
    ShotRow nextShotRow = getShotRow(index);

    if(index == 0) {
        //Special case for this row
        setTabOrder(currentRow.stationName(), previousRow.down(), nextRow.stationName());
        lrudTabNavigation(currentRow, nextShotRow.backClino(), nextRow.left());

    } else if(index == 1) {
        //Special case for this row
        setTabOrder(currentRow.stationName(), previousRow.stationName(), previousShotRow.distance());
        lrudTabNavigation(currentRow, previousRow.down(), nextRow.stationName());
    } else {
        setTabOrder(currentRow.stationName(), previousRow.down(), previousShotRow.distance());
        lrudTabNavigation(currentRow, previousShotRow.backClino(), nextRow.stationName());
    }
}

/**
  \brief Updates the tab order of the LRUD at index
  */
void cwSurveyChunkView::lrudTabNavigation(StationRow row, QDeclarativeItem* previous, QDeclarativeItem* next) {
    setTabOrder(row.left(), previous, row.right());
    setTabOrder(row.right(), row.left(), row.up());
    setTabOrder(row.up(), row.right(), row.down());
    setTabOrder(row.down(), row.up(), next);
}

/**
  \brief Updates the tab order for the shot
  */
void cwSurveyChunkView::updateShotTabNavigation(int index) {
    ShotRow row = getShotRow(index);
    StationRow fromStationRow = getStationRow(index);
    StationRow toStationRow = getStationRow(index + 1);

    setTabOrder(row.distance(), toStationRow.stationName(), row.frontCompass());
    setTabOrder(row.frontCompass(), row.distance(), row.backCompass());
    setTabOrder(row.backCompass(), row.frontCompass(), row.frontClino());
    setTabOrder(row.frontClino(), row.backCompass(), row.backClino());

    if(index == 0) {
        //Special case for this row
        setTabOrder(row.backClino(), row.frontClino(), fromStationRow.left());
    } else {
        setTabOrder(row.backClino(), row.frontClino(), toStationRow.left());
    }
}

void cwSurveyChunkView::setTabOrder(QDeclarativeItem* item, QDeclarativeItem* previous, QDeclarativeItem* next) {
    if(item == NULL) { return; }
    item->setProperty("previousTabObject", QVariant::fromValue(previous));
    item->setProperty("nextTabObject", QVariant::fromValue(next));
}

/**
  \brief Sets the navigation for the station item
  */
void cwSurveyChunkView::updateStationArrowNavigation(int index) {
    StationRow previousRow = getNavigationStationRow(index - 1);
    StationRow currentRow = getNavigationStationRow(index);
    StationRow nextRow = getNavigationStationRow(index + 1);

    ShotRow previousShotRow = getShotRow(index - 1);
    ShotRow nextShotRow = getShotRow(index);

    setArrowNavigation(currentRow.stationName(), NULL, previousShotRow.distance(), previousRow.stationName(), nextRow.stationName());
    setArrowNavigation(currentRow.left(), previousShotRow.backClino(), currentRow.right(), previousRow.left(), nextRow.left());
    setArrowNavigation(currentRow.right(), currentRow.left(), currentRow.up(), previousRow.right(), nextRow.right());
    setArrowNavigation(currentRow.up(), currentRow.right(), currentRow.down(), previousRow.up(), nextRow.up());
    setArrowNavigation(currentRow.down(), currentRow.up(), NULL, previousRow.down(), nextRow.down());

    if(index == 0) {
        //Special case
        setArrowNavigation(currentRow.stationName(), NULL, nextShotRow.distance(), previousRow.stationName(), nextRow.stationName());
        setArrowNavigation(currentRow.left(), nextShotRow.frontClino(), currentRow.right(), previousRow.left(), nextRow.left());
    }
}

/**
  \brief Sets the navigation for shot item
  */
void cwSurveyChunkView::updateShotArrowNavigaton(int index) {
    StationRow fromStationRow = getStationRow(index);
    StationRow toStationRow = getStationRow(index + 1);

    ShotRow previousRow = getNavigationShotRow(index - 1);
    ShotRow row = getNavigationShotRow(index);
    ShotRow nextRow = getNavigationShotRow(index + 1);

    setArrowNavigation(row.distance(), toStationRow.stationName(), row.frontCompass(), previousRow.distance(), nextRow.distance());
    setArrowNavigation(row.frontCompass(), row.distance(), row.frontClino(), previousRow.backCompass(), row.backCompass());
    setArrowNavigation(row.backCompass(), row.distance(), row.backClino(), row.frontCompass(), nextRow.frontCompass());
    setArrowNavigation(row.frontClino(), row.frontCompass(), fromStationRow.left(), previousRow.backClino(), row.backClino());
    setArrowNavigation(row.backClino(), row.backCompass(), toStationRow.left(), row.frontClino(), nextRow.frontClino());
}


/**
  \brief Sets the navigation for an item
  \param item - The item that the navigation will be set for
  \param left - The item to the left
  \param right - The item to the right
  \param up - The item above
  \param down - The item below
  */
void cwSurveyChunkView::setArrowNavigation(QDeclarativeItem* item, QDeclarativeItem* left, QDeclarativeItem* right, QDeclarativeItem* up, QDeclarativeItem* down) {
    if(item == NULL) { return; }
    item->setProperty("navLeftObject", QVariant::fromValue(left));
    item->setProperty("navRightObject", QVariant::fromValue(right));
    item->setProperty("navUpObject", QVariant::fromValue(up));
    item->setProperty("navDownObject", QVariant::fromValue(down));
}

/**
  \brief Get's the navigation station row for an index

  \param index - If the index is -1 the it'll get the last station in the ChunkAbove
  If the index is the same size of the the number of stations this will get the ChunkBelow
  */
cwSurveyChunkView::ShotRow cwSurveyChunkView::getNavigationShotRow(int index) {
    if(index == -1) {
        if(ChunkAbove != NULL && !ChunkAbove->ShotRows.isEmpty()) {
            return ChunkAbove->ShotRows.last();
        }
    } else if(index == ShotRows.size()) {
        if(ChunkBelow != NULL && !ChunkBelow->ShotRows.isEmpty()) {
            return ChunkBelow->ShotRows.first();
        }
    }

    return getShotRow(index);
}

/**
  \brief This creates a shot row for the index

  If the index is out of range then the ShotRow will invalid
  all elements will be null
  */
cwSurveyChunkView::ShotRow cwSurveyChunkView::getShotRow(int index) {
    return index >= 0 && index < ShotRows.size() ? ShotRows[index] : ShotRow();
}

/**
  \brief Get's the navigation station row for an index

  \param index - If the index is -1 the it'll get the last station in the ChunkAbove
  If the index is the same size of the the number of stations this will get the ChunkBelow
  */
cwSurveyChunkView::StationRow cwSurveyChunkView::getNavigationStationRow(int index) {
    if(index == -1) {
        if(ChunkAbove != NULL && !ChunkAbove->StationRows.isEmpty()) {
            return ChunkAbove->StationRows.last();
        }
    } else if(index == StationRows.size()) {
        if(ChunkBelow != NULL && !ChunkBelow->StationRows.isEmpty()) {
            return ChunkBelow->StationRows.first();
        }
    }

    return getStationRow(index);
}

/**
  \brief This creates a station row for the index

  If the index is out of range then the StationRow will be invalid
  all elements will be null
  */
cwSurveyChunkView::StationRow cwSurveyChunkView::getStationRow(int index) {
    if(index >= 0 && index < StationRows.size()) {
        return StationRows[index];
    } else {
        return StationRow();
    }
}

/**
  \brief This handles editor behaviour of the last rows
  */
void cwSurveyChunkView::updateLastRowBehaviour() {
    int lastIndex = StationRows.size() - 1;
    int secondToLastIndex = StationRows.size() - 2;
    StationRow lastRow = getStationRow(lastIndex);
    StationRow secondLastRow = getStationRow(secondToLastIndex);

    if(lastRow.stationName() != NULL) {
        connect(lastRow.stationName(), SIGNAL(dataValueChanged()), SLOT(stationValueHasChanged()));
    }

    if(secondLastRow.stationName() != NULL) {
        connect(secondLastRow.stationName(), SIGNAL(dataValueChanged()), SLOT(stationValueHasChanged()));
    }
}

/**
  \brief Updates the position in the view

  If index is 0 then all index's positions are update
  */
void cwSurveyChunkView::updatePositionsAfterIndex(int index) {
    //Update the position when we have a valid interface
    if(!interfaceValid()) { return; }

    for(int i = index; i < StationRows.size(); i++) {
        positionStationRow(StationRows[i], i);
    }

    for(int i = index; i < ShotRows.size(); i++) {
        positionShotRow(ShotRows[i], i);
    }
}

/**
  Updates all the indexes for the gui elements, this is so
  the right click works correctly
  */
void cwSurveyChunkView::updateIndexes(int index) {
    if(!interfaceValid()) { return; }

    for(int i = index; i < StationRows.size(); i++) {
        foreach(QDeclarativeItem* item, StationRows[i].items()) {
            item->setProperty("rowIndex", i);
        }
    }

    for(int i = index; i < ShotRows.size(); i++) {
        foreach(QDeclarativeItem* item, ShotRows[i].items()) {
            item->setProperty("rowIndex", i);
        }
    }

}

/**
  \brief Test to see if the interface is valid
  */
bool cwSurveyChunkView::interfaceValid() {
    if(StationRows.empty() || ShotRows.empty()) { return false; }
    if(StationRows.size() - 1 != ShotRows.size()) { return false; }
    return true;
}

/**
  \brief This updates the bounds of the view
  */
void cwSurveyChunkView::updateDimensions() {
    if(!interfaceValid()) { return; }

    QRectF rect = boundingRect();
    //qDebug() << "BoundingRect:" << rect;

    setWidth(rect.width());
    setHeight(rect.height());
}

/**
  \brief Updates the view's shot data.

  The role must by on of the following:
        ShotDistance,
        ShotCompassRole,
        ShotBackCompassRole,
        ShotClinoRole,
        ShotBackClinoRole

   If it's invalid, the this function does nothing

   If the index is invalid, this does nothing

   data - The data that the view will be updated with
  */
void cwSurveyChunkView::updateShotData(cwSurveyChunk::DataRole role, int index, const QVariant& data) {
    if(index < 0 || index >= ShotRows.size()) { return; }

    QDeclarativeItem* shotItem;
    switch(role) {
    case cwSurveyChunk::ShotDistanceRole:
        shotItem = ShotRows[index].distance();
        break;
    case cwSurveyChunk::ShotCompassRole:
        shotItem = ShotRows[index].frontCompass();
        break;
    case cwSurveyChunk::ShotBackCompassRole:
        shotItem = ShotRows[index].backCompass();
        break;
    case cwSurveyChunk::ShotClinoRole:
        shotItem = ShotRows[index].frontClino();
        break;
    case cwSurveyChunk::ShotBackClinoRole:
        shotItem = ShotRows[index].backClino();
        break;
    default:
        return;
    }

    shotItem->setProperty("dataValue", data);
}

/**
  \brief Updates the view's station data.

  The role must by on of the following:
        StationNameRole,
        StationLeftRole,
        StationRightRole,
        StationUpRole,
        StationDownRole,

   If it's invalid, the this function does nothing

   If the index is invalid, this does nothing

   data - The data that the view will be updated with
  */
void cwSurveyChunkView::updateStationData(cwSurveyChunk::DataRole role, int index, const QVariant& data) {
    if(index < 0 || index >= StationRows.size()) {
        return;
    }

    QDeclarativeItem* stationItem;
    switch(role) {
    case cwSurveyChunk::StationNameRole:
        stationItem = StationRows[index].stationName();
        break;
    case cwSurveyChunk::StationLeftRole:
        stationItem = StationRows[index].left();
        break;
    case cwSurveyChunk::StationRightRole:
        stationItem = StationRows[index].right();
        break;
    case cwSurveyChunk::StationUpRole:
        stationItem = StationRows[index].up();
        break;
    case cwSurveyChunk::StationDownRole:
        stationItem = StationRows[index].down();
        break;
    default:
        return;
    }

    stationItem->setProperty("dataValue", data);
}

/**
  This updates all the station data a index

  If index, is invalid, this does nothing
  */
void cwSurveyChunkView::updateStationRowData(int index) {
    if(index < 0 || index >= StationRows.size()) {
        return;
    }

    updateStationData(cwSurveyChunk::StationNameRole,
                      index,
                      SurveyChunk->data(cwSurveyChunk::StationNameRole, index));

    updateStationData(cwSurveyChunk::StationLeftRole,
                      index,
                      SurveyChunk->data(cwSurveyChunk::StationLeftRole, index));

    updateStationData(cwSurveyChunk::StationRightRole,
                      index,
                      SurveyChunk->data(cwSurveyChunk::StationRightRole, index));

    updateStationData(cwSurveyChunk::StationUpRole,
                      index,
                      SurveyChunk->data(cwSurveyChunk::StationUpRole, index));

    updateStationData(cwSurveyChunk::StationDownRole,
                      index,
                      SurveyChunk->data(cwSurveyChunk::StationDownRole, index));


}

/**
  This updates all the shot data at an index for a row

  If the index is invalid, this does nothing
  */
void cwSurveyChunkView::updateShotRowData(int index) {
    if(index < 0 || index >= ShotRows.size()) { return; }

    updateShotData(cwSurveyChunk::ShotDistanceRole,
                   index,
                   SurveyChunk->data(cwSurveyChunk::ShotDistanceRole, index));


    updateShotData(cwSurveyChunk::ShotCompassRole,
                   index,
                   SurveyChunk->data(cwSurveyChunk::ShotCompassRole, index));

    updateShotData(cwSurveyChunk::ShotBackCompassRole,
                   index,
                   SurveyChunk->data(cwSurveyChunk::ShotBackCompassRole, index));

    updateShotData(cwSurveyChunk::ShotClinoRole,
                   index,
                   SurveyChunk->data(cwSurveyChunk::ShotClinoRole, index));

    updateShotData(cwSurveyChunk::ShotBackClinoRole,
                   index,
                   SurveyChunk->data(cwSurveyChunk::ShotBackClinoRole, index));

}
