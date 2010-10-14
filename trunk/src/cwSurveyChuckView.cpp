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

cwSurveyChuckView::cwSurveyChuckView(QDeclarativeItem* parent) :
    QDeclarativeItem(parent),
    Model(NULL),
    StationDelegate(NULL),
    TitleDelegate(NULL),
    LeftDelegate(NULL),
    RightDelegate(NULL),
    UpDelegate(NULL),
    DownDelegate(NULL)
{


}

/**
  \brief Get's the model from the view
  */
cwSurveyChunk* cwSurveyChuckView::model() {
    return Model;
}

/**
  \brief Set's the model for the view
  */
void cwSurveyChuckView::setModel(cwSurveyChunk* chunk) {
    if(Model != chunk) {
        Model = chunk;

        SetupDelegates();

        Clear();
        AddStations(0, Model->StationCount() - 1);
        AddShots(0, Model->ShotCount() - 1);

        emit modelChanged();
    }
}

/**
  \brief Re-initializes the view
  */
void cwSurveyChuckView::Clear() {
    foreach(QGraphicsItem* item, childItems()) {
        delete item;
    }

    foreach(StationRow* row, StationRows) {
        delete row;
    }

    CreateTitlebar();
}

/**
  \brief Add stations to the view

  This inserts new stations found in the model at and between beginIndex and endIndex.
  */
void cwSurveyChuckView::AddStations(int beginIndex, int endIndex) {
    //Make sure the model has data
    if(Model->StationCount() == 0) { return; }

    //Make sure these are good indexes, else clamp
    beginIndex = qMin(0, beginIndex);
    endIndex = qMax(Model->StationCount() - 1, endIndex);

    for(int i = beginIndex; i <= endIndex; i++) {
        //Create the row
        StationRow* row = new StationRow(this);

        //Insert the row
        StationRows.insert(i, row);

        //Position the row in the correct place
        PositionStationRow(row, i);

        //Hock up the signals and slots with the models data
        cwStation* station = Model->Station(i);
        ConnectStation(station, row);
    }
}

/**
  \brief Adds shots to the view

  This inserts new shots found in the model at and between beginIndex and endIndex
  */
void cwSurveyChuckView::AddShots(int beginIndex, int endIndex) {
    //Make sure the model has data
    if(Model->ShotCount() == 0) { return; }

    //Make sure thes are good indexes, else clamp
    beginIndex = qMax(0, beginIndex);
    endIndex = qMin(Model->ShotCount() - 1, endIndex);

    for(int i = beginIndex; i <= endIndex; i++) {
        //Create the row
        ShotRow* row = new ShotRow(this);

        //Insert the row
        ShotRows.insert(i, row);

        //Position the row in the correct place
        PositionShotRow(row, i);

        //Hock up the signals and slots with the model's data
        cwShot* shot = Model->Shot(i);
        ConnectShot(shot, row);
    }
}

/**
  \brief Creates the title bar for the view
  */
void cwSurveyChuckView::CreateTitlebar() {

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

    StationTitle->setPos(10, 10);
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
void cwSurveyChuckView::SetupDelegates() {
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
  \brief Constructor to the StationRow
  */
cwSurveyChuckView::StationRow::StationRow(cwSurveyChuckView* view) {
    Station = qobject_cast<QDeclarativeItem*>(view->StationDelegate->create());
    Left = qobject_cast<QDeclarativeItem*>(view->LeftDelegate->create());
    Right = qobject_cast<QDeclarativeItem*>(view->RightDelegate->create());
    Up = qobject_cast<QDeclarativeItem*>(view->UpDelegate->create());
    Down = qobject_cast<QDeclarativeItem*>(view->DownDelegate->create());

    Station->setParentItem(view);
    Left->setParentItem(view);
    Right->setParentItem(view);
    Up->setParentItem(view);
    Down->setParentItem(view);
}

/**
  \brief Constructor to the ShotRow
  */
cwSurveyChuckView::ShotRow::ShotRow(cwSurveyChuckView *view) {
    Distance = qobject_cast<QDeclarativeItem*>(view->DistanceDelegate->create());
    FrontCompass = qobject_cast<QDeclarativeItem*>(view->FrontCompassDelegate->create());
    BackCompass = qobject_cast<QDeclarativeItem*>(view->BackCompassDelegate->create());
    FrontClino = qobject_cast<QDeclarativeItem*>(view->FrontClinoDelegate->create());
    BackClino = qobject_cast<QDeclarativeItem*>(view->BackClinoDelegate->create());

    Distance->setParentItem(view);
    FrontCompass->setParentItem(view);
    BackCompass->setParentItem(view);
    FrontClino->setParentItem(view);
    BackClino->setParentItem(view);
}

/**
  \brief This positions the station row

  Will position the row based no the model's index of the row.
  All the elements in station row will be position correctly
  */
void cwSurveyChuckView::PositionStationRow(StationRow* row, int index) {
    PositionElement(row->station(), StationTitle, index);
    PositionElement(row->left(), LeftTitle, index);
    PositionElement(row->right(), RightTitle, index);
    PositionElement(row->up(), UpTitle, index);
    PositionElement(row->down(), DownTitle, index);
}

/**
  \brief Helper to the PositionStationRow
  */
void cwSurveyChuckView::PositionElement(QDeclarativeItem* item, QDeclarativeItem* titleItem, int index, int yOffset, QSizeF size) {
    QRectF titleRect = mapRectFromItem(titleItem, titleItem->boundingRect());
    size = !size.isValid() ? titleRect.size() + QSizeF(-2, 0) : size;
    float y = titleRect.bottom() + titleRect.height() * index;
    item->setPos(titleRect.left() + 1, yOffset + y - 1);
    item->setSize(size);
}

/**
  \brief Hooks up the model to the qml, for the station row
  */
void cwSurveyChuckView::ConnectStation(cwStation* station, StationRow* row) {
    QVariant stationObject = QVariant::fromValue(static_cast<QObject*>(station));

    row->station()->setProperty("dataObject", stationObject);
    row->left()->setProperty("dataObject", stationObject);
    row->right()->setProperty("dataObject", stationObject);
    row->up()->setProperty("dataObject", stationObject);
    row->down()->setProperty("dataObject", stationObject);
}

/**
  \brief Positions the shot row in the object
  */
void cwSurveyChuckView::PositionShotRow(ShotRow* row, int index) {
    PositionElement(row->distance(), DistanceTitle, index);

    float halfAzimuthHeight = AzimuthTitle->height() / 2.0 + 1.0;
    float halfClinoHeight = ClinoTitle->height() / 2.0 + 1.0;
    PositionElement(row->frontCompass(), AzimuthTitle, index, 0, QSize(AzimuthTitle->width(), halfAzimuthHeight));
    PositionElement(row->backCompass(), AzimuthTitle, index, halfAzimuthHeight, QSize(AzimuthTitle->width(), halfAzimuthHeight));
    PositionElement(row->frontClino(), ClinoTitle, index, 0, QSize(ClinoTitle->width(), halfClinoHeight));
    PositionElement(row->backClino(), ClinoTitle, index, halfClinoHeight, QSize(ClinoTitle->width(), halfClinoHeight));
}

/**
  \brief Hooks up the model to the qml, for the shot row
  */
void cwSurveyChuckView::ConnectShot(cwShot* shot, ShotRow* row) {
    QVariant shotObject = QVariant::fromValue(static_cast<QObject*>(shot));

    row->distance()->setProperty("dataObject", shotObject);
    row->frontCompass()->setProperty("dataObject", shotObject);
    row->backCompass()->setProperty("dataObject", shotObject);
    row->frontClino()->setProperty("dataObject", shotObject);
    row->backClino()->setProperty("dataObject", shotObject);
}

