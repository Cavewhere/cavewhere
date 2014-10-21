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

//Qt includes
#include <QSharedDataPointer>

enum class SurveyDataType {
    StandardCaveData,
    DiveData,
};

class cwShot
{

public:
    cwShot();

    cwShot(SurveyDataType type, QString distance,
           QString compass, QString backCompass,
           QString typeData1, QString typeData2);

    cwShot(const cwShot& shot);

    double distance() const;
    void setDistance(QString distance);
    void setDistance(double distance);
    void setDistanceState(cwDistanceStates::State state);

    double compass() const;
    void setCompass(QString compass);
    void setCompass(double compass);
    void setCompassState(cwCompassStates::State state);

    double backCompass() const;
    void setBackCompass(QString backCompass);
    void setBackCompass(double backCompass);
    void setBackCompassState(cwCompassStates::State state);

    double clino() const;
    void setClino(QString backClino);
    void setClino(double clino);
    void setClinoState(cwClinoStates::State state);

    double backClino() const;
    void setBackClino(QString backClino);
    void setBackClino(double backClino);
    void setBackClinoState(cwClinoStates::State state);

    double fromDepth() const;
    void setFromDepth(QString depth);
    void setFromDepth(double depth);
    void setFromDepthState(cwDepthStates::State state);

    double toDepth() const;
    void setToDepth(QString depth);
    void setToDepth(double depth);
    void setToDepthState(cwDepthStates::State state);

    cwDistanceStates::State distanceState() const;
    cwCompassStates::State compassState() const;
    cwCompassStates::State backCompassState() const;
    cwClinoStates::State clinoState() const;
    cwClinoStates::State backClinoState() const;
    cwDepthStates::State fromDepthState() const;
    cwDepthStates::State toDepthState() const;

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
        PrivateData(SurveyDataType dataType=SurveyDataType::StandardCaveData);

        double Distance;
        double Compass;
        double BackCompass;
        double Clino;
        double BackClino;
        double FromDepth;
        double ToDepth;

        SurveyDataType surveyDataType;

        cwDistanceStates::State DistanceState;
        cwCompassStates::State CompassState;
        cwCompassStates::State BackCompassState;
        cwClinoStates::State ClinoState;
        cwClinoStates::State BackClinoState;
        cwDepthStates::State FromDepthState;
        cwDepthStates::State ToDepthState;

        bool IncludeDistance;
    };

    QSharedDataPointer<PrivateData> Data;

    ValidState setValueWithString(const cwValidator& validator, double& memberData, int& memberState, QString newValue);
    void setClinoValueWithString(double& memberData, int& memberState, QString newValue);
    void setPrivateCompass(double& memberData, cwCompassStates::State& state, double value);
    void setPrivateClino(double& memberData, cwClinoStates::State& state, double value);

    void setPrivateCompassState(cwCompassStates::State& memberState, cwCompassStates::State newState);
    void setPrivateClinoState(cwClinoStates::State& memberState, cwClinoStates::State newState);

    void setDepthValueWithString(double& memberData, int& memberState, QString newValue);
    void setPrivateDepth(double &memberData, cwDepthStates::State &state, double value);
    void setPrivateDepthState(cwDepthStates::State& memberState, cwDepthStates::State newState);
};

inline cwDistanceStates::State cwShot::distanceState() const {
    return Data->DistanceState;
}

inline cwCompassStates::State cwShot::compassState() const {
    return Data->CompassState;
}

inline cwCompassStates::State cwShot::backCompassState() const {
    return Data->BackCompassState;
}

inline cwClinoStates::State cwShot::clinoState() const {
    return Data->ClinoState;
}

inline cwClinoStates::State cwShot::backClinoState() const {
    return Data->BackClinoState;
}

inline cwDepthStates::State cwShot::fromDepthState() const {
    return Data->FromDepthState;
}

inline cwDepthStates::State cwShot::toDepthState() const {
    return Data->ToDepthState;
}

/**
 * @brief cwShot::isDistanceIncluded
 * @return True if the shot should be included in the length or false if it should be excluded
 */
inline bool cwShot::isDistanceIncluded() const
{
    return Data->IncludeDistance;
}

inline double cwShot::distance() const {
    return Data->Distance;
}

inline double cwShot::compass() const {
    return Data->Compass;
}

inline double cwShot::backCompass() const {
    return Data->BackCompass;
}

inline double cwShot::clino() const {
    return Data->Clino;
}

inline double cwShot::backClino() const {
    return Data->BackClino;
}

inline double cwShot::fromDepth() const {
    return Data->FromDepth;
}

inline double cwShot::toDepth() const {
    return Data->ToDepth;
}

inline bool cwShot::isValid() const {
    return distanceState() == cwDistanceStates::Valid;
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
