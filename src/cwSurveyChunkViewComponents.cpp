/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurveyChunkViewComponents.h"
#include "cwStationValidator.h"
#include "cwDistanceValidator.h"
#include "cwClinoValidator.h"
#include "cwCompassValidator.h"
#include "cwGlobalDirectory.h"
#include "cwDebug.h"

//Qt includes
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QDebug>

cwSurveyChunkViewComponents::cwSurveyChunkViewComponents(QQmlContext* context, QObject *parent) :
    QObject(parent)
{
    QQmlEngine* engine = context->engine();

    DataBoxDelegate = new QQmlComponent(engine, cwGlobalDirectory::baseDirectory() + "qml/DataBox.qml", this);
    StationDelegate = new QQmlComponent(engine, cwGlobalDirectory::baseDirectory() + "qml/StationBox.qml", this);
    TitleDelegate = new QQmlComponent(engine, cwGlobalDirectory::baseDirectory() + "qml/TitleLabel.qml", this);
    FrontSiteDelegate = new QQmlComponent(engine, cwGlobalDirectory::baseDirectory() + "qml/FrontSightReadingBox.qml", this);
    BackSiteDelegate = new QQmlComponent(engine, cwGlobalDirectory::baseDirectory() + "qml/BackSightReadingBox.qml", this);
    ShotDistanceDelegate = new QQmlComponent(engine, cwGlobalDirectory::baseDirectory() + "qml/ShotDistanceDataBox.qml", this);
    ErrorDelegate = new QQmlComponent(engine, cwGlobalDirectory::baseDirectory() + "qml/SurveyChunkErrorDelegate.qml", this);

    //Print error if there are any
    cwDebug::printErrors(DataBoxDelegate);
    cwDebug::printErrors(StationDelegate);
    cwDebug::printErrors(TitleDelegate);
    cwDebug::printErrors(FrontSiteDelegate);
    cwDebug::printErrors(BackSiteDelegate);
    cwDebug::printErrors(ShotDistanceDelegate);

    StationValidator = new cwStationValidator(this);
    DistanceValidator = new cwDistanceValidator(this);
    ClinoValidator = new cwClinoValidator(this);
    CompassValidator = new cwCompassValidator(this);

}


