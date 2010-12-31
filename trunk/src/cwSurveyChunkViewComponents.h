#ifndef CWSURVEYCHUNKVIEWCOMPONENTS_H
#define CWSURVEYCHUNKVIEWCOMPONENTS_H

//This make one global object for cwSurveyChunkView components
//This should reduce the memory footprint of a cwSurveyChunkview
//And reducte the

//Qt includes
#include <QObject>
class QDeclarativeComponent;
class QDeclarativeContext;

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

signals:

public slots:

private:
    QDeclarativeComponent* StationDelegate;
    QDeclarativeComponent* TitleDelegate;
    QDeclarativeComponent* LeftDelegate;
    QDeclarativeComponent* RightDelegate;
    QDeclarativeComponent* UpDelegate;
    QDeclarativeComponent* DownDelegate;
    QDeclarativeComponent* DistanceDelegate;
    QDeclarativeComponent* FrontCompassDelegate;
    QDeclarativeComponent* BackCompassDelegate;
    QDeclarativeComponent* FrontClinoDelegate;
    QDeclarativeComponent* BackClinoDelegate;

    void printErrors(QDeclarativeComponent* component);

};

inline QDeclarativeComponent* cwSurveyChunkViewComponents::titleDelegate() const {
    return TitleDelegate;
}

inline QDeclarativeComponent* cwSurveyChunkViewComponents::stationDelegate() const {
    return StationDelegate;
}

inline QDeclarativeComponent* cwSurveyChunkViewComponents::leftDelegate() const {
    return LeftDelegate;
}

inline QDeclarativeComponent* cwSurveyChunkViewComponents::rightDelegate() const {
    return RightDelegate;
}

inline QDeclarativeComponent* cwSurveyChunkViewComponents::upDelegate() const {
    return UpDelegate;
}

inline QDeclarativeComponent* cwSurveyChunkViewComponents::downDelegate() const {
    return DownDelegate;
}

inline QDeclarativeComponent* cwSurveyChunkViewComponents::distanceDelegate() const {
    return DistanceDelegate;
}

inline QDeclarativeComponent* cwSurveyChunkViewComponents::frontCompassDelegate() const {
    return FrontCompassDelegate;
}

inline QDeclarativeComponent* cwSurveyChunkViewComponents::backCompassDelegate() const {
    return BackCompassDelegate;
}

inline QDeclarativeComponent* cwSurveyChunkViewComponents::frontClinoDelegate() const {
    return FrontClinoDelegate;
}

inline QDeclarativeComponent* cwSurveyChunkViewComponents::backClinoDelegate() const {
    return BackClinoDelegate;
}

#endif // CWSURVEYCHUNKVIEWCOMPONENTS_H
