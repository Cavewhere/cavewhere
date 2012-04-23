//Our includes
#include "cwSurveyChunkViewComponents.h"
#include "cwStationValidator.h"
#include "cwDistanceValidator.h"
#include "cwClinoValidator.h"
#include "cwCompassValidator.h"
#include "cwGlobalDirectory.h"

//Qt includes
#include <QDeclarativeContext>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QDebug>

cwSurveyChunkViewComponents::cwSurveyChunkViewComponents(QDeclarativeContext* context, QObject *parent) :
    QObject(parent)
{
    QQmlEngine* engine = context->engine();

    Delegate = new QQmlComponent(engine, cwGlobalDirectory::baseDirectory() + "qml/DataBox.qml", this);
    StationDelegate = new QQmlComponent(engine, cwGlobalDirectory::baseDirectory() + "qml/StationBox.qml", this);
    TitleDelegate = new QQmlComponent(engine, cwGlobalDirectory::baseDirectory() + "qml/TitleLabel.qml", this);
    FrontSiteDelegate = new QQmlComponent(engine, cwGlobalDirectory::baseDirectory() + "qml/FrontSightReadingBox.qml", this);
    BackSiteDelegate = new QQmlComponent(engine, cwGlobalDirectory::baseDirectory() + "qml/BackSightReadingBox.qml", this);
    //    LeftDelegate = new QQmlComponent(engine, "qml/LeftDataBox.qml", this);
//    RightDelegate = new QQmlComponent(engine, "qml/RightDataBox.qml", this);
//    UpDelegate = new QQmlComponent(engine, "qml/UpDataBox.qml", this);
//    DownDelegate = new QQmlComponent(engine, "qml/DownDataBox.qml", this);
//    DistanceDelegate = new QQmlComponent(engine, "qml/ShotDistanceDataBox.qml", this);
//    FrontCompassDelegate = new QQmlComponent(engine, "qml/FrontCompassReadBox.qml", this);
//    BackCompassDelegate = new QQmlComponent(engine, "qml/BackCompassReadBox.qml", this);
//    FrontClinoDelegate = new QQmlComponent(engine, "qml/FrontClinoReadBox.qml", this);
//    BackClinoDelegate = new QQmlComponent(engine, "qml/BackClinoReadBox.qml", this);

    //Print error if there are any
    printErrors(Delegate);
    printErrors(StationDelegate);
    printErrors(TitleDelegate);
//    printErrors(LeftDelegate);
//    printErrors(RightDelegate);
//    printErrors(UpDelegate);
//    printErrors(DownDelegate);
//    printErrors(DistanceDelegate);
//    printErrors(FrontCompassDelegate);
//    printErrors(BackCompassDelegate);
//    printErrors(FrontClinoDelegate);
//    printErrors(BackClinoDelegate);

    StationValidator = new cwStationValidator(this);
    DistanceValidator = new cwDistanceValidator(this);
    ClinoValidator = new cwClinoValidator(this);
    CompassValidator = new cwCompassValidator(this);

}

/**
  \brief Prints errors for cwSurveyChunkViewComponents
 */
void cwSurveyChunkViewComponents::printErrors(QQmlComponent* component) {
    if(!component->errors().isEmpty()) {
        qDebug() << component->errorString();
    }
}
