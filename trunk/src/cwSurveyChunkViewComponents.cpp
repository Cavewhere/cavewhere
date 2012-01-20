//Our includes
#include "cwSurveyChunkViewComponents.h"
#include "cwStationValidator.h"
#include "cwDistanceValidator.h"
#include "cwClinoValidator.h"
#include "cwCompassValidator.h"
#include "cwGlobalDirectory.h"

//Qt includes
#include <QDeclarativeContext>
#include <QDeclarativeEngine>
#include <QDeclarativeComponent>
#include <QDebug>

cwSurveyChunkViewComponents::cwSurveyChunkViewComponents(QDeclarativeContext* context, QObject *parent) :
    QObject(parent)
{
    QDeclarativeEngine* engine = context->engine();

    Delegate = new QDeclarativeComponent(engine, cwGlobalDirectory::baseDirectory() + "qml/DataBox.qml", this);
    StationDelegate = new QDeclarativeComponent(engine, "qml/StationBox.qml", this);
    TitleDelegate = new QDeclarativeComponent(engine, cwGlobalDirectory::baseDirectory() + "qml/TitleLabel.qml", this);
    FrontSiteDelegate = new QDeclarativeComponent(engine, cwGlobalDirectory::baseDirectory() + "qml/FrontSightReadingBox.qml", this);
    BackSiteDelegate = new QDeclarativeComponent(engine, cwGlobalDirectory::baseDirectory() + "qml/BackSightReadingBox.qml", this);
    //    LeftDelegate = new QDeclarativeComponent(engine, "qml/LeftDataBox.qml", this);
//    RightDelegate = new QDeclarativeComponent(engine, "qml/RightDataBox.qml", this);
//    UpDelegate = new QDeclarativeComponent(engine, "qml/UpDataBox.qml", this);
//    DownDelegate = new QDeclarativeComponent(engine, "qml/DownDataBox.qml", this);
//    DistanceDelegate = new QDeclarativeComponent(engine, "qml/ShotDistanceDataBox.qml", this);
//    FrontCompassDelegate = new QDeclarativeComponent(engine, "qml/FrontCompassReadBox.qml", this);
//    BackCompassDelegate = new QDeclarativeComponent(engine, "qml/BackCompassReadBox.qml", this);
//    FrontClinoDelegate = new QDeclarativeComponent(engine, "qml/FrontClinoReadBox.qml", this);
//    BackClinoDelegate = new QDeclarativeComponent(engine, "qml/BackClinoReadBox.qml", this);

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
void cwSurveyChunkViewComponents::printErrors(QDeclarativeComponent* component) {
    if(!component->errors().isEmpty()) {
        qDebug() << component->errorString();
    }
}
