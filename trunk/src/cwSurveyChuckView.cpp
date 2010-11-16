//Our includes
#include "cwSurveyChuckView.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"

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
    Model(NULL),
    StationDelegate(NULL),
    TitleDelegate(NULL),
    LeftDelegate(NULL),
    RightDelegate(NULL),
    UpDelegate(NULL),
    DownDelegate(NULL),
    FocusedItem(NULL),
    ChunkAbove(NULL),
    ChunkBelow(NULL)
{
    connect(this, SIGNAL(focusChanged(bool)), SLOT(SetFocusForFirstStation(bool)));

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
void cwSurveyChunkView::SetNavigationBelow(const cwSurveyChunkView* below) {
    ChunkBelow = below;

    if(StationRows.empty()) { return; }

    //Update first station navigation
    int lastIndex = StationRows.size() - 1;
    UpdateStationTabNavigation(lastIndex);
    UpdateStationArrowNavigation(lastIndex);

    lastIndex = ShotRows.size() - 1;
    UpdateShotArrowNavigaton(lastIndex);
}

/**
  \brief Sets the navigation for this object

  If the user use the keyboard to move the focus, above is the object above this
  object.  This will update the navigation for the first last object in this view
  */
void cwSurveyChunkView::SetNavigationAbove(const cwSurveyChunkView* above) {
    ChunkAbove = above;

    if(StationRows.empty()) { return; }

    UpdateStationTabNavigation(0);
    UpdateStationArrowNavigation(0);
    UpdateShotArrowNavigaton(0);
}

/**
  \brief Convenience method

  Equivalent to SetNavigationBelow and SetNavigationAbove
  */
void cwSurveyChunkView::SetNavigation(const cwSurveyChunkView* above, const cwSurveyChunkView* below) {
    SetNavigationAbove(above);
    SetNavigationBelow(below);
}

/**
  \brief Get's the model from the view
  */
cwSurveyChunk* cwSurveyChunkView::model() {
    return Model;
}

/**
  \brief Set's the model for the view
  */
void cwSurveyChunkView::setModel(cwSurveyChunk* chunk) {
    if(Model != chunk) {
        Model = chunk;

        SetupDelegates();

        Clear();
        AddStations(0, Model->StationCount() - 1);
        AddShots(0, Model->ShotCount() - 1);

        connect(Model, SIGNAL(ShotsAdded(int,int)), SLOT(AddShots(int,int)));
        connect(Model, SIGNAL(StationsAdded(int,int)), SLOT(AddStations(int,int)));
        connect(Model, SIGNAL(ShotsRemoved(int,int)), SLOT(RemoveShots(int,int)));
        connect(Model, SIGNAL(StationsRemoved(int,int)), SLOT(RemoveStations(int,int)));

        emit modelChanged();
    }
}

QRectF cwSurveyChunkView::boundingRect() const {
    return childrenBoundingRect();
}

/**
  \brief Re-initializes the view
  */
void cwSurveyChunkView::Clear() {
    foreach(QGraphicsItem* item, childItems()) {
        delete item;
    }

    StationRows.clear();
    ShotRows.clear();

    CreateTitlebar();
}

/**
  \brief User right clicks on station
  */
void cwSurveyChunkView::RightClickOnStation(int index) {

    QMenu menu;

    QAction* insertAbove = menu.addAction(QIcon(":icons/AddArrowUp.png"), "Insert Above");
    QAction* insertBelow = menu.addAction(QIcon(":icons/AddArrowDown.png"),"Insert Below");
    menu.addSeparator();
    QAction* removeAbove = menu.addAction(QIcon(":icons/RemoveArrowUp.png"),"Remove Above");
    QAction* removeBelow = menu.addAction(QIcon(":icons/RemoveArrowDown.png"),"Remove Below");

    removeAbove->setEnabled(Model->CanRemoveStation(index, cwSurveyChunk::Above));
    removeBelow->setEnabled(Model->CanRemoveStation(index, cwSurveyChunk::Below));

    QAction* selected = menu.exec(QCursor::pos());
    if(selected == insertAbove) {
        Model->InsertStation(index, cwSurveyChunk::Above);
    } else if(selected == insertBelow) {
        Model->InsertStation(index, cwSurveyChunk::Below);
    } else if(selected == removeAbove) {
        Model->RemoveStation(index, cwSurveyChunk::Above);
    } else if(selected == removeBelow) {
        Model->RemoveStation(index, cwSurveyChunk::Below);
    }
}

/**
  \brief User right clicks on station
  */
void cwSurveyChunkView::RightClickOnShot(int index) {
    QMenu menu;

    QAction* insertAbove = menu.addAction(QIcon(":icons/AddArrowUp.png"), "Insert Above");
    QAction* insertBelow = menu.addAction(QIcon(":icons/AddArrowDown.png"),"Insert Below");
    menu.addSeparator();
    QAction* removeAbove = menu.addAction(QIcon(":icons/RemoveArrowUp.png"),"Remove Above");
    QAction* removeBelow = menu.addAction(QIcon(":icons/RemoveArrowDown.png"),"Remove Below");

    removeAbove->setEnabled(Model->CanRemoveShot(index, cwSurveyChunk::Above));
    removeBelow->setEnabled(Model->CanRemoveShot(index, cwSurveyChunk::Below));

    QAction* selected = menu.exec(QCursor::pos());
    if(selected == insertAbove) {
        Model->InsertShot(index, cwSurveyChunk::Above);
    } else if(selected == insertBelow) {
        Model->InsertShot(index, cwSurveyChunk::Below);
    } else if(selected == removeAbove) {
        Model->RemoveShot(index, cwSurveyChunk::Above);
    } else if(selected == removeBelow) {
        Model->RemoveShot(index, cwSurveyChunk::Below);
    }
}


/**
  \brief Splits the chunk into two pieces on a station

  \param int - The station index where the station will be
  split

  This will emit a InsertNewChunk()
  */
void cwSurveyChunkView::SplitOnStation(int index) {
    qDebug() << "cwSurveyChunkView: split on station" << index;

    //Make sure the index is good
    if(index < 0 || index >= Model->StationCount()) {
        return; //Can't split on a bad index
    }

    cwSurveyChunk* newChunk = Model->SplitAtStation(index);
    emit createdNewChunk(newChunk);
}

/**
  \brief Splits the chunk into two pieces on a station

  \param int - The shot index where the chunk will be split
*/
void cwSurveyChunkView::SplitOnShot(int index) {



}


/**
  \brief Add stations to the view

  This inserts new stations found in the model at and between beginIndex and endIndex.
  */
void cwSurveyChunkView::AddStations(int beginIndex, int endIndex) {
    //Make sure the model has data
    if(Model->StationCount() == 0) { return; }

    //Alert the scene graph that we're changing
    prepareGeometryChange();

    //Disconnect stations that were watched
    disconnect(this, SLOT(StationValueHasChanged()));

    //Make sure these are good indexes, else clamp
    beginIndex = qMax(0, beginIndex);
    endIndex = qMin(Model->StationCount() - 1, endIndex);

    for(int i = beginIndex; i <= endIndex; i++) {
        //Create the row
        StationRow row(this, i);

        //Insert the row
        StationRows.insert(i, row);

        //Position the row in the correct place
        PositionStationRow(row, i);

        //Hock up the signals and slots with the models data
        cwStation* station = Model->Station(i);
        ConnectStation(station, row);

        //Queue the index for navigation update
        StationNavigationQueue.append(i);
    }

    //Add previous station and next station to the navigation queue
    StationNavigationQueue.append(beginIndex - 1);
    StationNavigationQueue.append(endIndex + 1);

    //Update the navigation
    UpdateNavigation();

    //Connect the last station's in the view
    UpdateLastRowBehaviour();

    //Update the positions for all rows after endIndex
    UpdatePositionsAfterIndex(endIndex + 1);

    //Update the id's on effected rows
    UpdateIndexes(endIndex + 1);

    UpdateDimensions();

    //qDebug() << boundingRect();
}

/**
  \brief Adds shots to the view

  This inserts new shots found in the model at and between beginIndex and endIndex
  */
void cwSurveyChunkView::AddShots(int beginIndex, int endIndex) {
    //Make sure the model has data
    if(Model->ShotCount() == 0) { return; }

    //Alert the scene graph that we're changing
    prepareGeometryChange();

    //Make sure thes are good indexes, else clamp
    beginIndex = qMax(0, beginIndex);
    endIndex = qMin(Model->ShotCount() - 1, endIndex);

    for(int i = beginIndex; i <= endIndex; i++) {
        //Create the row
        ShotRow row(this, i);

        //Insert the row
        ShotRows.insert(i, row);

        //Position the row in the correct place
        PositionShotRow(row, i);

        //Hock up the signals and slots with the model's data
        cwShot* shot = Model->Shot(i);
        ConnectShot(shot, row);

        //Queue the index for navigation update
        ShotNavigationQueue.append(i);
    }

    //Add previous station and next station to the navigation queue
    ShotNavigationQueue.append(beginIndex - 1);
    ShotNavigationQueue.append(endIndex + 1);
    UpdateNavigation();

    //Update the positions for all rows after endIndex
    UpdatePositionsAfterIndex(endIndex + 1);

    //Update the id's on effected rows
    UpdateIndexes(endIndex + 1);

    UpdateDimensions();

}

/**
  \brief Removes stations from the view
  */
void cwSurveyChunkView::RemoveStations(int beginIndex, int endIndex) {

    //Make sure the index are good
    beginIndex = qMax(0, beginIndex);
    endIndex = qMin(StationRows.size() - 1, endIndex);

    for(int i = endIndex; i >= beginIndex; i--) {
        StationRow row = GetStationRow(i);

        foreach(QDeclarativeItem* item, row.items()) {
            if(item) {
                item->deleteLater();
            }
        }

        //Remove the row from the station rows
        StationRows.removeAt(i);
    }

    //Update the navigation for the remaining rows
    StationNavigationQueue.append(beginIndex - 1);
    StationNavigationQueue.append(beginIndex);
    StationNavigationQueue.append(beginIndex + 1);
    UpdateNavigation();

    //Update the positions for all rows after endIndex
    UpdatePositionsAfterIndex(beginIndex);

    //Update the id's on effected rows
    UpdateIndexes(beginIndex);

    UpdateDimensions();
}

/**
  \brief Remove shots from the view
  */
void cwSurveyChunkView::RemoveShots(int beginIndex, int endIndex) {
    //Make sure the index are good
    beginIndex = qMax(0, beginIndex);
    endIndex = qMin(ShotRows.size() - 1, endIndex);

    for(int i = endIndex; i >= beginIndex; i--) {
        ShotRow row = GetShotRow(i);

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

    UpdateNavigation();

    //Update the positions for all rows after endIndex
    UpdatePositionsAfterIndex(beginIndex);

    //Update the id's on effected rows
    UpdateIndexes(beginIndex);

    UpdateDimensions();
}


/**
  \brief Creates the title bar for the view
  */
void cwSurveyChunkView::CreateTitlebar() {

    StationTitle = qobject_cast<QDeclarativeItem*>(TitleDelegate->create());
    DistanceTitle = qobject_cast<QDeclarativeItem*>(TitleDelegate->create());
    AzimuthTitle = qobject_cast<QDeclarativeItem*>(TitleDelegate->create());
    ClinoTitle = qobject_cast<QDeclarativeItem*>(TitleDelegate->create());
    LeftTitle = qobject_cast<QDeclarativeItem*>(TitleDelegate->create());
    RightTitle = qobject_cast<QDeclarativeItem*>(TitleDelegate->create());
    UpTitle = qobject_cast<QDeclarativeItem*>(TitleDelegate->create());
    DownTitle = qobject_cast<QDeclarativeItem*>(TitleDelegate->create());

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
  Sets up the QML components that are used to create QML objects in
  this view
  */
void cwSurveyChunkView::SetupDelegates() {
    QDeclarativeContext* context = QDeclarativeEngine::contextForObject(this);
    if(context == NULL) { return; }

    QDeclarativeEngine* engine = context->engine();
    if(engine == NULL) { return; }


    if(StationDelegate == NULL) {
        StationDelegate = new QDeclarativeComponent(engine, "qml/StationBox.qml", this);
        TitleDelegate = new QDeclarativeComponent(engine, "qml/TitleLabel.qml", this);
        LeftDelegate = new QDeclarativeComponent(engine, "qml/LeftDataBox.qml", this);
        RightDelegate = new QDeclarativeComponent(engine, "qml/RightDataBox.qml", this);
        UpDelegate = new QDeclarativeComponent(engine, "qml/UpDataBox.qml", this);
        DownDelegate = new QDeclarativeComponent(engine, "qml/DownDataBox.qml", this);
        DistanceDelegate = new QDeclarativeComponent(engine, "qml/ShotDistanceDataBox.qml", this);
        FrontCompassDelegate = new QDeclarativeComponent(engine, "qml/FrontCompassReadBox.qml", this);
        BackCompassDelegate = new QDeclarativeComponent(engine, "qml/BackCompassReadBox.qml", this);
        FrontClinoDelegate = new QDeclarativeComponent(engine, "qml/FrontClinoReadBox.qml", this);
        BackClinoDelegate = new QDeclarativeComponent(engine, "qml/BackClinoReadBox.qml", this);
    }
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
    Items[Station] = qobject_cast<QDeclarativeItem*>(view->StationDelegate->create());
    Items[Left] = qobject_cast<QDeclarativeItem*>(view->LeftDelegate->create());
    Items[Right] = qobject_cast<QDeclarativeItem*>(view->RightDelegate->create());
    Items[Up] = qobject_cast<QDeclarativeItem*>(view->UpDelegate->create());
    Items[Down] = qobject_cast<QDeclarativeItem*>(view->DownDelegate->create());

    foreach(QDeclarativeItem* item, items()) {
        item->setParentItem(view);
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
  \brief Constructor for a shot row
  */
cwSurveyChunkView::ShotRow::ShotRow() : Row(-1, NumberItems)
{

}

/**
  \brief Constructor to the ShotRow
  */
cwSurveyChunkView::ShotRow::ShotRow(cwSurveyChunkView *view, int rowIndex) : Row(rowIndex, NumberItems) {
    Items[Distance] = qobject_cast<QDeclarativeItem*>(view->DistanceDelegate->create());
    Items[FrontCompass] = qobject_cast<QDeclarativeItem*>(view->FrontCompassDelegate->create());
    Items[BackCompass] = qobject_cast<QDeclarativeItem*>(view->BackCompassDelegate->create());
    Items[FrontClino] = qobject_cast<QDeclarativeItem*>(view->FrontClinoDelegate->create());
    Items[BackClino] = qobject_cast<QDeclarativeItem*>(view->BackClinoDelegate->create());

    foreach(QDeclarativeItem* item, items()) {
        item->setParentItem(view);
        item->setParent(view);
    }
}

/**
  \brief This positions the station row

  Will position the row based no the model's index of the row.
  All the elements in station row will be position correctly
  */
void cwSurveyChunkView::PositionStationRow(StationRow row, int index) {
    PositionElement(row.station(), StationTitle, index);
    PositionElement(row.left(), LeftTitle, index);
    PositionElement(row.right(), RightTitle, index);
    PositionElement(row.up(), UpTitle, index);
    PositionElement(row.down(), DownTitle, index);
}

/**
  \brief Helper to the PositionStationRow
  */
void cwSurveyChunkView::PositionElement(QDeclarativeItem* item, QDeclarativeItem* titleItem, int index, int yOffset, QSizeF size) {
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
void cwSurveyChunkView::ConnectStation(cwStation* station, StationRow row) {
    QVariant stationObject = QVariant::fromValue(static_cast<QObject*>(station));

    QVector<QDeclarativeItem*> items = row.items();
    foreach(QDeclarativeItem* item, items) {
        item->setProperty("dataObject", stationObject);
        item->setProperty("rowIndex", row.rowIndex());
        connect(item, SIGNAL(rightClicked(int)), SLOT(RightClickOnStation(int)));
        connect(item, SIGNAL(splitOn(int)), SLOT(SplitOnStation(int)));
        connect(item, SIGNAL(focusChanged(bool)), SLOT(SetChildActiveFocus(bool)));
    }
}

/**
  \brief Positions the shot row in the object
  */
void cwSurveyChunkView::PositionShotRow(ShotRow row, int index) {
    PositionElement(row.distance(), DistanceTitle, index);

    float halfAzimuthHeight = AzimuthTitle->height() / 2.0 + 1.0;
    float halfClinoHeight = ClinoTitle->height() / 2.0 + 1.0;
    PositionElement(row.frontCompass(), AzimuthTitle, index, 0, QSize(AzimuthTitle->width(), halfAzimuthHeight));
    PositionElement(row.backCompass(), AzimuthTitle, index, halfAzimuthHeight, QSize(AzimuthTitle->width(), halfAzimuthHeight));
    PositionElement(row.frontClino(), ClinoTitle, index, 0, QSize(ClinoTitle->width(), halfClinoHeight));
    PositionElement(row.backClino(), ClinoTitle, index, halfClinoHeight, QSize(ClinoTitle->width(), halfClinoHeight));
}

/**
  \brief Hooks up the model to the qml, for the shot row
  */
void cwSurveyChunkView::ConnectShot(cwShot* shot, ShotRow row) {
    QVariant shotObject = QVariant::fromValue(static_cast<QObject*>(shot));

    QVector<QDeclarativeItem*> items = row.items();
    foreach(QDeclarativeItem* item, items) {
        item->setProperty("dataObject", shotObject);
        item->setProperty("rowIndex", row.rowIndex());
        connect(item, SIGNAL(rightClicked(int)), SLOT(RightClickOnShot(int)));
        connect(item, SIGNAL(splitOn(int)), SLOT(SplitOnShot(int)));
        connect(item, SIGNAL(focusChanged(bool)), SLOT(SetChildActiveFocus(bool)));
    }
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
void cwSurveyChunkView::SetFocusForFirstStation(bool focus) {
    //If this view has focus
    if(focus && !StationRows.isEmpty()) {
        StationRow row = StationRows.first();
        row.station()->setFocus(true); //Focus the first station
    }
}

/**
  \brief Called when the children focus has changed
  */
void cwSurveyChunkView::SetChildActiveFocus(bool focus) {
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
void cwSurveyChunkView::StationValueHasChanged() {
    int stationCount = Model->StationCount();
    if(stationCount < 2) { return; }

    StationRow lastStation = GetStationRow(stationCount - 1);
    StationRow secondLastStation = GetStationRow(stationCount - 2);

    if(lastStation.station() == NULL || secondLastStation.station() == NULL) { return; }

    QString lastStationName = lastStation.station()->property("dataValue").toString();
    QString secondLastStationName = secondLastStation.station()->property("dataValue").toString();


    if(lastStationName.isEmpty() && secondLastStationName.isEmpty()) {
        ShotRow lastShotRow = GetShotRow(Model->ShotCount() - 1);

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

            Model->RemoveStation(Model->StationCount() - 1, cwSurveyChunk::Above);
        }


    } else if(!lastStationName.isEmpty()) {
        //Add station to the model
        Model->AppendNewShot();
    }
}



/**
  \brief Updates the navigation

  Updates the navigation for pending elements in the StationNavigationQueue and
  the ShotNavigationQueue and there neighbors
  */
void cwSurveyChunkView::UpdateNavigation() {

    //Valid that we can update navigation
    if(StationRows.empty() || ShotRows.empty()) { return; }
    if(StationRows.size() - 1 != ShotRows.size()) { return; }

    foreach(int index, StationNavigationQueue) {
        UpdateStationTabNavigation(index);
        UpdateStationArrowNavigation(index);
    }

    foreach(int index, ShotNavigationQueue) {
        UpdateShotTabNavigation(index);
        UpdateShotArrowNavigaton(index);
    }

    StationNavigationQueue.clear();
    ShotNavigationQueue.clear();
}

/**
  \brief Helper function to Update Navigation

  Update the tab order for the staton row at index
  */
void cwSurveyChunkView::UpdateStationTabNavigation(int index) {

    StationRow previousRow = GetNavigationStationRow(index - 1);
    StationRow currentRow = GetNavigationStationRow(index);
    StationRow nextRow = GetNavigationStationRow(index + 1);

    ShotRow previousShotRow = GetShotRow(index - 1);
    ShotRow nextShotRow = GetShotRow(index);

    if(index == 0) {
        //Special case for this row
        SetTabOrder(currentRow.station(), previousRow.down(), nextRow.station());
        LRUDTabNavigation(currentRow, nextShotRow.backClino(), nextRow.left());

    } else if(index == 1) {
        //Special case for this row
        SetTabOrder(currentRow.station(), previousRow.station(), previousShotRow.distance());
        LRUDTabNavigation(currentRow, previousRow.down(), nextRow.station());
    } else {
        SetTabOrder(currentRow.station(), previousRow.down(), previousShotRow.distance());
        LRUDTabNavigation(currentRow, previousShotRow.backClino(), nextRow.station());
    }
}

/**
  \brief Updates the tab order of the LRUD at index
  */
void cwSurveyChunkView::LRUDTabNavigation(StationRow row, QDeclarativeItem* previous, QDeclarativeItem* next) {
    SetTabOrder(row.left(), previous, row.right());
    SetTabOrder(row.right(), row.left(), row.up());
    SetTabOrder(row.up(), row.right(), row.down());
    SetTabOrder(row.down(), row.up(), next);
}

/**
  \brief Updates the tab order for the shot
  */
void cwSurveyChunkView::UpdateShotTabNavigation(int index) {
    ShotRow row = GetShotRow(index);
    StationRow fromStationRow = GetStationRow(index);
    StationRow toStationRow = GetStationRow(index + 1);

    SetTabOrder(row.distance(), toStationRow.station(), row.frontCompass());
    SetTabOrder(row.frontCompass(), row.distance(), row.backCompass());
    SetTabOrder(row.backCompass(), row.frontCompass(), row.frontClino());
    SetTabOrder(row.frontClino(), row.backCompass(), row.backClino());

    if(index == 0) {
        //Special case for this row
        SetTabOrder(row.backClino(), row.frontClino(), fromStationRow.left());
    } else {
        SetTabOrder(row.backClino(), row.frontClino(), toStationRow.left());
    }
}

void cwSurveyChunkView::SetTabOrder(QDeclarativeItem* item, QDeclarativeItem* previous, QDeclarativeItem* next) {
    if(item == NULL) { return; }
    item->setProperty("previousTabObject", QVariant::fromValue(previous));
    item->setProperty("nextTabObject", QVariant::fromValue(next));
}

/**
  \brief Sets the navigation for the station item
  */
void cwSurveyChunkView::UpdateStationArrowNavigation(int index) {
    StationRow previousRow = GetNavigationStationRow(index - 1);
    StationRow currentRow = GetNavigationStationRow(index);
    StationRow nextRow = GetNavigationStationRow(index + 1);

    ShotRow previousShotRow = GetShotRow(index - 1);
    ShotRow nextShotRow = GetShotRow(index);

    SetArrowNavigation(currentRow.station(), NULL, previousShotRow.distance(), previousRow.station(), nextRow.station());
    SetArrowNavigation(currentRow.left(), previousShotRow.backClino(), currentRow.right(), previousRow.left(), nextRow.left());
    SetArrowNavigation(currentRow.right(), currentRow.left(), currentRow.up(), previousRow.right(), nextRow.right());
    SetArrowNavigation(currentRow.up(), currentRow.right(), currentRow.down(), previousRow.up(), nextRow.up());
    SetArrowNavigation(currentRow.down(), currentRow.up(), NULL, previousRow.down(), nextRow.down());

    if(index == 0) {
        //Special case
        SetArrowNavigation(currentRow.station(), NULL, nextShotRow.distance(), previousRow.station(), nextRow.station());
        SetArrowNavigation(currentRow.left(), nextShotRow.frontClino(), currentRow.right(), previousRow.left(), nextRow.left());
    }
}

/**
  \brief Sets the navigation for shot item
  */
void cwSurveyChunkView::UpdateShotArrowNavigaton(int index) {
    StationRow fromStationRow = GetStationRow(index);
    StationRow toStationRow = GetStationRow(index + 1);

    ShotRow previousRow = GetNavigationShotRow(index - 1);
    ShotRow row = GetNavigationShotRow(index);
    ShotRow nextRow = GetNavigationShotRow(index + 1);

    SetArrowNavigation(row.distance(), toStationRow.station(), row.frontCompass(), previousRow.distance(), nextRow.distance());
    SetArrowNavigation(row.frontCompass(), row.distance(), row.frontClino(), previousRow.backCompass(), row.backCompass());
    SetArrowNavigation(row.backCompass(), row.distance(), row.backClino(), row.frontCompass(), nextRow.frontCompass());
    SetArrowNavigation(row.frontClino(), row.frontCompass(), fromStationRow.left(), previousRow.backClino(), row.backClino());
    SetArrowNavigation(row.backClino(), row.backCompass(), toStationRow.left(), row.frontClino(), nextRow.frontClino());
}


/**
  \brief Sets the navigation for an item
  \param item - The item that the navigation will be set for
  \param left - The item to the left
  \param right - The item to the right
  \param up - The item above
  \param down - The item below
  */
void cwSurveyChunkView::SetArrowNavigation(QDeclarativeItem* item, QDeclarativeItem* left, QDeclarativeItem* right, QDeclarativeItem* up, QDeclarativeItem* down) {
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
cwSurveyChunkView::ShotRow cwSurveyChunkView::GetNavigationShotRow(int index) {
    if(index == -1) {
        if(ChunkAbove != NULL && !ChunkAbove->ShotRows.isEmpty()) {
            return ChunkAbove->ShotRows.last();
        }
    } else if(index == ShotRows.size()) {
        if(ChunkBelow != NULL && !ChunkBelow->ShotRows.isEmpty()) {
            return ChunkBelow->ShotRows.first();
        }
    }

    return GetShotRow(index);
}

/**
  \brief This creates a shot row for the index

  If the index is out of range then the ShotRow will invalid
  all elements will be null
  */
cwSurveyChunkView::ShotRow cwSurveyChunkView::GetShotRow(int index) {
    return index >= 0 && index < ShotRows.size() ? ShotRows[index] : ShotRow();
}

/**
  \brief Get's the navigation station row for an index

  \param index - If the index is -1 the it'll get the last station in the ChunkAbove
  If the index is the same size of the the number of stations this will get the ChunkBelow
  */
cwSurveyChunkView::StationRow cwSurveyChunkView::GetNavigationStationRow(int index) {
    if(index == -1) {
        if(ChunkAbove != NULL && !ChunkAbove->StationRows.isEmpty()) {
            return ChunkAbove->StationRows.last();
        }
    } else if(index == StationRows.size()) {
        if(ChunkBelow != NULL && !ChunkBelow->StationRows.isEmpty()) {
            return ChunkBelow->StationRows.first();
        }
    }

    return GetStationRow(index);
}

/**
  \brief This creates a station row for the index

  If the index is out of range then the StationRow will be invalid
  all elements will be null
  */
cwSurveyChunkView::StationRow cwSurveyChunkView::GetStationRow(int index) {
    if(index >= 0 && index < StationRows.size()) {
        return StationRows[index];
    } else {
        return StationRow();
    }
}

/**
  \brief This handles editor behaviour of the last rows
  */
void cwSurveyChunkView::UpdateLastRowBehaviour() {
    int lastIndex = StationRows.size() - 1;
    int secondToLastIndex = StationRows.size() - 2;
    StationRow lastRow = GetStationRow(lastIndex);
    StationRow secondLastRow = GetStationRow(secondToLastIndex);

    if(lastRow.station() != NULL) {
        connect(lastRow.station(), SIGNAL(dataValueChanged()), SLOT(StationValueHasChanged()));
    }

    if(secondLastRow.station() != NULL) {
        connect(secondLastRow.station(), SIGNAL(dataValueChanged()), SLOT(StationValueHasChanged()));
    }
}

/**
  \brief Updates the position in the view

  If index is 0 then all index's positions are update
  */
void cwSurveyChunkView::UpdatePositionsAfterIndex(int index) {
    //Update the position when we have a valid interface
    if(!InterfaceValid()) { return; }

    for(int i = index; i < StationRows.size(); i++) {
        PositionStationRow(StationRows[i], i);
    }

    for(int i = index; i < ShotRows.size(); i++) {
        PositionShotRow(ShotRows[i], i);
    }
}

/**
  Updates all the indexes for the gui elements, this is so
  the right click works correctly
  */
void cwSurveyChunkView::UpdateIndexes(int index) {
    if(!InterfaceValid()) { return; }

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
bool cwSurveyChunkView::InterfaceValid() {
    if(StationRows.empty() || ShotRows.empty()) { return false; }
    if(StationRows.size() - 1 != ShotRows.size()) { return false; }
    return true;
}

void cwSurveyChunkView::UpdateDimensions() {
    if(!InterfaceValid()) { return; }

    QRectF rect = boundingRect();
    //qDebug() << "BoundingRect:" << rect;

    setWidth(rect.width());
    setHeight(rect.height());
}
