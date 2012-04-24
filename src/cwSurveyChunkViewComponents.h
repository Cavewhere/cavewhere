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

    cwValidator* stationValidator() const;
    cwValidator* lrudValidator() const;
    cwValidator* distanceValidator() const;
    cwValidator* compassValidator() const;
    cwValidator* clinoValidator() const;

signals:

public slots:

private:
    QQmlComponent* Delegate;
    QQmlComponent* StationDelegate;
    QQmlComponent* TitleDelegate;
    QQmlComponent* FrontSiteDelegate;
    QQmlComponent* BackSiteDelegate;

    cwStationValidator* StationValidator;
    cwDistanceValidator* DistanceValidator;
    cwCompassValidator* CompassValidator;
    cwClinoValidator* ClinoValidator;


    void printErrors(QQmlComponent* component);

};

inline QQmlComponent* cwSurveyChunkViewComponents::titleDelegate() const {
    return TitleDelegate;
}

inline QQmlComponent* cwSurveyChunkViewComponents::stationDelegate() const {
    //return Delegate;
    return StationDelegate;
}

inline QQmlComponent* cwSurveyChunkViewComponents::leftDelegate() const {
    return Delegate;
    //    return LeftDelegate;
}

inline QQmlComponent* cwSurveyChunkViewComponents::rightDelegate() const {
    return Delegate;
    //    return RightDelegate;
}

inline QQmlComponent* cwSurveyChunkViewComponents::upDelegate() const {
    return Delegate;
    //    return UpDelegate;
}

inline QQmlComponent* cwSurveyChunkViewComponents::downDelegate() const {
    return Delegate;
    //    return DownDelegate;
}

inline QQmlComponent* cwSurveyChunkViewComponents::distanceDelegate() const {
    return Delegate;
    //    return DistanceDelegate;
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
