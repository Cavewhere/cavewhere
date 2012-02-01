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

    cwValidator* stationValidator() const;
    cwValidator* lrudValidator() const;
    cwValidator* distanceValidator() const;
    cwValidator* compassValidator() const;
    cwValidator* clinoValidator() const;

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
