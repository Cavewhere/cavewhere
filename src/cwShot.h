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
#include "cwLengthInput.h"
#include "cwAngleInput.h"
#include "cwVerticalAngleInput.h"

//Qt includes
#include <QSharedDataPointer>

class CAVEWHERE_LIB_EXPORT cwShot
{

public:
    cwShot();

    cwShot(QString distance,
           QString compass, QString backCompass,
           QString clino, QString backClino);
    cwShot(const cwShot& shot);

    cwLengthInput distance() const;
    void setDistance(QString distance);
    void setDistance(double distance);

    cwAngleInput compass() const;
    void setCompass(QString compass);
    void setCompass(double compass);

    cwAngleInput backCompass() const;
    void setBackCompass(QString backCompass);
    void setBackCompass(double backCompass);

    cwVerticalAngleInput clino() const;
    void setClino(QString backClino);
    void setClino(double clino);

    cwVerticalAngleInput backClino() const;
    void setBackClino(QString backClino);
    void setBackClino(double backClino);

    bool isDistanceIncluded() const;
    void setDistanceIncluded(bool isDistanceIncluded);

    bool isValid() const;

    bool sameInternalPointer(const cwShot& other) const;

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

        cwLengthInput Distance;
        cwAngleInput Compass;
        cwAngleInput BackCompass;
        cwVerticalAngleInput Clino;
        cwVerticalAngleInput BackClino;

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

inline cwLengthInput cwShot::distance() const {
    return Data->Distance;
}

inline cwAngleInput cwShot::compass() const {
    return Data->Compass;
}

inline cwAngleInput cwShot::backCompass() const {
    return Data->BackCompass;
}

inline cwVerticalAngleInput cwShot::clino() const {
    return Data->Clino;
}

inline cwVerticalAngleInput cwShot::backClino() const {
    return Data->BackClino;
}

inline bool cwShot::isValid() const {
    return Data->Distance.isValid();
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
inline bool cwShot::sameInternalPointer(const cwShot& other) const {
    return Data == other.Data;
}

#endif // CSHOT_H
