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
class QDeclarativeComponent;
class QDeclarativeContext;
class QValidator;


class cwSurveyChunkViewComponents : public QObject
{
    Q_OBJECT
public:
    explicit cwSurveyChunkViewComponents(QDeclarativeContext* context, QObject *parent = 0);

    QDeclarativeComponent* titleDelegate() const;
    QDeclarativeComponent* stationDelegate() const;
    QDeclarativeComponent* leftDelegate() const;
    QDeclarativeComponent* rightDelegate() const;
    QDeclarativeComponent* upDelegate() const;
    QDeclarativeComponent* downDelegate() const;
    QDeclarativeComponent* distanceDelegate() const;
    QDeclarativeComponent* frontCompassDelegate() const;
    QDeclarativeComponent* backCompassDelegate() const;
    QDeclarativeComponent* frontClinoDelegate() const;
    QDeclarativeComponent* backClinoDelegate() const;

    QValidator* stationValidator() const;
    QValidator* lrudValidator() const;
    QValidator* distanceValidator() const;
    QValidator* compassValidator() const;
    QValidator* clinoValidator() const;

    QVariant stationValidatorAsVariant() const;
    QVariant lrudValidatorAsVariant() const;
    QVariant distanceValidatorAsVariant() const;
    QVariant compassValidatorAsVariant() const;
    QVariant clinoValidatorAsVariant() const;

signals:

public slots:

private:
    QDeclarativeComponent* Delegate;
    QDeclarativeComponent* StationDelegate;
    QDeclarativeComponent* TitleDelegate;
    QDeclarativeComponent* FrontSiteDelegate;
    QDeclarativeComponent* BackSiteDelegate;

    cwStationValidator* StationValidator;
    cwDistanceValidator* DistanceValidator;
    cwCompassValidator* CompassValidator;
    cwClinoValidator* ClinoValidator;


    void printErrors(QDeclarativeComponent* component);

};

inline QDeclarativeComponent* cwSurveyChunkViewComponents::titleDelegate() const {
    return TitleDelegate;
}

inline QDeclarativeComponent* cwSurveyChunkViewComponents::stationDelegate() const {
    //return Delegate;
    return StationDelegate;
}

inline QDeclarativeComponent* cwSurveyChunkViewComponents::leftDelegate() const {
    return Delegate;
    //    return LeftDelegate;
}

inline QDeclarativeComponent* cwSurveyChunkViewComponents::rightDelegate() const {
    return Delegate;
    //    return RightDelegate;
}

inline QDeclarativeComponent* cwSurveyChunkViewComponents::upDelegate() const {
    return Delegate;
    //    return UpDelegate;
}

inline QDeclarativeComponent* cwSurveyChunkViewComponents::downDelegate() const {
    return Delegate;
    //    return DownDelegate;
}

inline QDeclarativeComponent* cwSurveyChunkViewComponents::distanceDelegate() const {
    return Delegate;
    //    return DistanceDelegate;
}

inline QDeclarativeComponent* cwSurveyChunkViewComponents::frontCompassDelegate() const {
    return FrontSiteDelegate;
}

inline QDeclarativeComponent* cwSurveyChunkViewComponents::backCompassDelegate() const {
    return BackSiteDelegate;
}

inline QDeclarativeComponent* cwSurveyChunkViewComponents::frontClinoDelegate() const {
    return FrontSiteDelegate;
}

inline QDeclarativeComponent* cwSurveyChunkViewComponents::backClinoDelegate() const {
    return BackSiteDelegate;
}

inline QValidator* cwSurveyChunkViewComponents::stationValidator() const {
    return StationValidator;
}

inline QValidator* cwSurveyChunkViewComponents::lrudValidator() const  {
    return DistanceValidator;
}

inline QValidator* cwSurveyChunkViewComponents::distanceValidator() const  {
    return DistanceValidator;
}

inline QValidator* cwSurveyChunkViewComponents::compassValidator() const  {
    return CompassValidator;
}

inline QValidator* cwSurveyChunkViewComponents::clinoValidator() const   {
    return ClinoValidator;
}

inline QVariant cwSurveyChunkViewComponents::stationValidatorAsVariant() const   {
    return QVariant::fromValue(static_cast<QObject*>(stationValidator()));
}

inline QVariant cwSurveyChunkViewComponents::lrudValidatorAsVariant() const   {
    return QVariant::fromValue(static_cast<QObject*>(lrudValidator()));
}

inline QVariant cwSurveyChunkViewComponents::distanceValidatorAsVariant() const   {
    return QVariant::fromValue(static_cast<QObject*>(distanceValidator()));
}

inline QVariant cwSurveyChunkViewComponents::compassValidatorAsVariant() const   {
    return QVariant::fromValue(static_cast<QObject*>(compassValidator()));
}

inline QVariant cwSurveyChunkViewComponents::clinoValidatorAsVariant() const   {
    return QVariant::fromValue(static_cast<QObject*>(clinoValidator()));
}


#endif // CWSURVEYCHUNKVIEWCOMPONENTS_H
