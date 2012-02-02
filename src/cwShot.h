#ifndef CSHOT_H
#define CSHOT_H

//Our includes
class cwSurveyChunk;
class cwStation;
class cwStationReference;
class cwValidator;
#include "cwReadingStates.h"

//Qt includes
#include <QVariant>

class cwShot
{

public:
    explicit cwShot();

    cwShot(QString distance,
           QString compass, QString backCompass,
           QString clino, QString backClino);

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

    cwDistanceStates::State distanceState() const;
    cwCompassStates::State compassState() const;
    cwCompassStates::State backCompassState() const;
    cwClinoStates::State clinoState() const;
    cwClinoStates::State backClinoState() const;

    void setParentChunk(cwSurveyChunk* parentChunk);
    cwSurveyChunk* parentChunk() const;

    cwStationReference toStation() const;
    cwStationReference fromStation() const;

private:
    enum ValidState {
        Invalid,
        ValidEmpty,
        ValidDouble,
        ValidString
    };


    double Distance;
    double Compass;
    double BackCompass;
    double Clino;
    double BackClino;

    cwDistanceStates::State DistanceState;
    cwCompassStates::State CompassState;
    cwCompassStates::State BackCompassState;
    cwClinoStates::State ClinoState;
    cwClinoStates::State BackClinoState;

    cwSurveyChunk* ParentChunk;

    ValidState setValueWithString(const cwValidator& validator, double& memberData, int& memberState, QString newValue);
    void setClinoValueWithString(double& memberData, int& memberState, QString newValue);
    void setPrivateCompass(double& memberData, cwCompassStates::State& state, double value);
    void setPrivateClino(double& memberData, cwClinoStates::State& state, double value);

    void setPrivateCompassState(cwCompassStates::State& memberState, cwCompassStates::State newState);
    void setPrivateClinoState(cwClinoStates::State& memberState, cwClinoStates::State newState);

};

inline cwDistanceStates::State cwShot::distanceState() const {
    return DistanceState;
}

inline cwCompassStates::State cwShot::compassState() const {
    return CompassState;
}

inline cwCompassStates::State cwShot::backCompassState() const {
    return BackCompassState;
}

inline cwClinoStates::State cwShot::clinoState() const {
    return ClinoState;
}

inline cwClinoStates::State cwShot::backClinoState() const {
    return BackClinoState;
}

inline double cwShot::distance() const {
    return Distance;
}

inline double cwShot::compass() const {
    return Compass;
}

inline double cwShot::backCompass() const {
    return BackCompass;
}

inline double cwShot::clino() const {
    return Clino;
}

inline double cwShot::backClino() const {
    return BackClino;
}


#endif // CSHOT_H
