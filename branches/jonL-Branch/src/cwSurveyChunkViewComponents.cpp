//Our includes
#include "cwSurveyChunkViewComponents.h"

//Qt includes
#include <QDeclarativeContext>
#include <QDeclarativeEngine>
#include <QDeclarativeComponent>
#include <QDebug>

cwSurveyChunkViewComponents::cwSurveyChunkViewComponents(QDeclarativeContext* context, QObject *parent) :
    QObject(parent)
{
    QDeclarativeEngine* engine = context->engine();
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

    //Print error if there are any
    printErrors(StationDelegate);
    printErrors(TitleDelegate);
    printErrors(LeftDelegate);
    printErrors(RightDelegate);
    printErrors(UpDelegate);
    printErrors(DownDelegate);
    printErrors(DistanceDelegate);
    printErrors(FrontCompassDelegate);
    printErrors(BackCompassDelegate);
    printErrors(FrontClinoDelegate);
    printErrors(BackClinoDelegate);
}

/**
  \brief Prints errors for cwSurveyChunkViewComponents
 */
void cwSurveyChunkViewComponents::printErrors(QDeclarativeComponent* component) {
    if(!component->errors().isEmpty()) {
        qDebug() << component->errorString();
    }
}
