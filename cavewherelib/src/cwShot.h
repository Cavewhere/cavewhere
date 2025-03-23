/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CSHOT_H
#define CSHOT_H

//Our includes
class cwSurveyChunk;
class cwStation;
class cwStationReference;
class cwValidator;
#include "cwReadingStates.h"
#include "cwGlobals.h"
#include "cwDistanceReading.h"
#include "cwCompassReading.h"
#include "cwClinoReading.h"

//Qt includes
#include <QSharedDataPointer>

class CAVEWHERE_LIB_EXPORT cwShot
{

public:
    cwShot();

    cwShot(const QString& distance,
           const QString& compass, const QString& backCompass,
           const QString& clino, const QString& backClino);
    cwShot(const cwShot& shot);

    cwDistanceReading distance() const { return Data->distance; }
    void setDistance(const cwDistanceReading& reading) { Data->distance = reading; }

    cwCompassReading compass() const { return Data->compass; }
    void setCompass(const cwCompassReading& reading) { Data->compass = reading; }

    cwCompassReading backCompass() const { return Data->backCompass; }
    void setBackCompass(const cwCompassReading& reading) { Data->backCompass = reading; }

    cwClinoReading clino() const { return Data->clino; }
    void setClino(const cwClinoReading& reading) { Data->clino = reading; }

    cwClinoReading backClino() const { return Data->backClino; }
    void setBackClino(const cwClinoReading& reading) { Data->backClino = reading; }

    bool isDistanceIncluded() const;
    void setDistanceIncluded(bool isDistanceIncluded);

    bool isValid() const;

    bool sameIntervalPointer(const cwShot& other) const;

private:
    enum ValidState {
        Invalid,
        ValidEmpty,
        ValidDouble,
        ValidString
    };

    class PrivateData : public QSharedData {
    public:
        PrivateData();

        cwDistanceReading distance;
        cwCompassReading compass;
        cwCompassReading backCompass;
        cwClinoReading clino;
        cwClinoReading backClino;

        bool IncludeDistance;
    };

    QSharedDataPointer<PrivateData> Data;

    ValidState setValueWithString(const cwValidator& validator, double& memberData, int& memberState, QString newValue);
    void setClinoValueWithString(double& memberData, int& memberState, QString newValue);
    void setPrivateCompass(double& memberData, cwCompassStates::State& state, double value);
    void setPrivateClino(double& memberData, cwClinoStates::State& state, double value);

    void setPrivateCompassState(cwCompassStates::State& memberState, cwCompassStates::State newState);
    void setPrivateClinoState(cwClinoStates::State& memberState, cwClinoStates::State newState);

};

/**
 * @brief cwShot::isDistanceIncluded
 * @return True if the shot should be included in the length or false if it should be excluded
 */
inline bool cwShot::isDistanceIncluded() const
{
    return Data->IncludeDistance;
}

inline bool cwShot::isValid() const {
    return distance().state() == cwDistanceReading::State::Valid;
}

/**
 * @brief cwShot::setDistanceIncluded
 * @param includeDistance
 * If true the shot will be included in the distance caluclation, otherwise, it'll be
 * excluded.
 *
 * By default this is true.
 */
inline void cwShot::setDistanceIncluded(bool includeDistance)
{
    Data->IncludeDistance = includeDistance;
}


/**
  This return's true if the other shot has the same intertal point as this shot
  */
inline bool cwShot::sameIntervalPointer(const cwShot& other) const {
    return Data == other.Data;
}

#endif // CSHOT_H
