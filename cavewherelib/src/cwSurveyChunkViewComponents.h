/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEYCHUNKVIEWCOMPONENTS_H
#define CWSURVEYCHUNKVIEWCOMPONENTS_H

//This make one global object for cwSurveyChunkView components
//This should reduce the memory footprint of a cwSurveyChunkview
//And reducte the

//Our includes
#include "cwStationValidator.h"
#include "cwDistanceValidator.h"
#include "cwCompassValidator.h"
#include "cwClinoValidator.h"

//Qt includes
#include <QObject>
#include <QVariant>
class QQmlComponent;
class QQmlContext;
class QValidator;


class cwSurveyChunkViewComponents : public QObject
{
    Q_OBJECT
public:
    explicit cwSurveyChunkViewComponents(QQmlContext* context, QObject *parent = 0);

    QQmlComponent* titleDelegate() const;
    QQmlComponent* stationDelegate() const;
    QQmlComponent* leftDelegate() const;
    QQmlComponent* rightDelegate() const;
    QQmlComponent* upDelegate() const;
    QQmlComponent* downDelegate() const;
    QQmlComponent* distanceDelegate() const;
    QQmlComponent* frontCompassDelegate() const;
    QQmlComponent* backCompassDelegate() const;
    QQmlComponent* frontClinoDelegate() const;
    QQmlComponent* backClinoDelegate() const;
    QQmlComponent* removeDataDelegate() const;
    QQmlComponent* errorDelegate() const;

    cwValidator* stationValidator() const;
    cwValidator* lrudValidator() const;
    cwValidator* distanceValidator() const;
    cwValidator* compassValidator() const;
    cwValidator* clinoValidator() const;

signals:

public slots:

private:
    QQmlComponent* DataBoxDelegate;
    QQmlComponent* StationDelegate;
    QQmlComponent* TitleDelegate;
    QQmlComponent* FrontSiteDelegate;
    QQmlComponent* BackSiteDelegate;
    QQmlComponent* ShotDistanceDelegate;
    QQmlComponent* RemoveDataDelegate;
    QQmlComponent* ErrorDelegate;

    cwStationValidator* StationValidator;
    cwDistanceValidator* DistanceValidator;
    cwCompassValidator* CompassValidator;
    cwClinoValidator* ClinoValidator;


};

inline QQmlComponent* cwSurveyChunkViewComponents::titleDelegate() const {
    return TitleDelegate;
}

inline QQmlComponent* cwSurveyChunkViewComponents::stationDelegate() const {
    return StationDelegate;
}

inline QQmlComponent* cwSurveyChunkViewComponents::leftDelegate() const {
    return DataBoxDelegate;
}

inline QQmlComponent* cwSurveyChunkViewComponents::rightDelegate() const {
    return DataBoxDelegate;
}

inline QQmlComponent* cwSurveyChunkViewComponents::upDelegate() const {
    return DataBoxDelegate;
}

inline QQmlComponent* cwSurveyChunkViewComponents::downDelegate() const {
    return DataBoxDelegate;
}

inline QQmlComponent* cwSurveyChunkViewComponents::distanceDelegate() const {
    return ShotDistanceDelegate;
}

inline QQmlComponent* cwSurveyChunkViewComponents::frontCompassDelegate() const {
    return FrontSiteDelegate;
}

inline QQmlComponent* cwSurveyChunkViewComponents::backCompassDelegate() const {
    return BackSiteDelegate;
}

inline QQmlComponent* cwSurveyChunkViewComponents::frontClinoDelegate() const {
    return FrontSiteDelegate;
}

inline QQmlComponent* cwSurveyChunkViewComponents::backClinoDelegate() const {
    return BackSiteDelegate;
}

inline QQmlComponent *cwSurveyChunkViewComponents::errorDelegate() const
{
    return ErrorDelegate;
}

inline cwValidator *cwSurveyChunkViewComponents::stationValidator() const {
    return StationValidator;
}

inline cwValidator *cwSurveyChunkViewComponents::lrudValidator() const  {
    return DistanceValidator;
}

inline cwValidator* cwSurveyChunkViewComponents::distanceValidator() const  {
    return DistanceValidator;
}

inline cwValidator* cwSurveyChunkViewComponents::compassValidator() const  {
    return CompassValidator;
}

inline cwValidator *cwSurveyChunkViewComponents::clinoValidator() const   {
    return ClinoValidator;
}

#endif // CWSURVEYCHUNKVIEWCOMPONENTS_H
