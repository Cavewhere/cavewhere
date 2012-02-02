//Our includes
#include "cwShot.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwStationReference.h"
#include "cwDistanceValidator.h"
#include "cwCompassValidator.h"
#include "cwClinoValidator.h"
#include "cwValidator.h"


cwShot::cwShot() :
    Distance(0.0),
    Compass(0.0),
    BackCompass(0.0),
    Clino(0.0),
    BackClino(0.0),
    DistanceState(cwDistanceStates::Empty),
    CompassState(cwCompassStates::Empty),
    BackCompassState(cwCompassStates::Empty),
    ClinoState(cwClinoStates::Empty),
    BackClinoState(cwClinoStates::Empty),
    ParentChunk(NULL) {
}

cwShot::cwShot(QString distance,
               QString compass,
               QString backCompass,
               QString clino,
               QString backClino) :
    ParentChunk(NULL)
{

    setDistance(distance);
    setCompass(compass);
    setBackCompass(backCompass);
    setClino(clino);
    setBackClino(backClino);
    ParentChunk = NULL;
}

void cwShot::setDistance(QString distance) {
    setValueWithString(cwDistanceValidator(), Distance, (int&)DistanceState, distance);
}

void cwShot::setDistance(double distance) {
    if(cwDistanceValidator::check(distance)) {
        Distance = distance;
        DistanceState = cwDistanceStates::Valid;
    }
}

void cwShot::setDistanceState(cwDistanceStates::State state)
{
    //We need this switch statement because serialization class (or any other class) could send in interger
    switch(state) {
    case cwDistanceStates::Empty:
        DistanceState = cwDistanceStates::Empty;
        break;
    case cwDistanceStates::Valid:
        DistanceState = cwDistanceStates::Valid;
        break;
    default:
        DistanceState = cwDistanceStates::Empty;
        break;
    }
}

void cwShot::setCompass(QString compass) {
    setValueWithString(cwCompassValidator(), Compass, (int&)CompassState, compass);
}

/// Sets the front sight compass for the shot
void cwShot::setCompass(double compass)
{
    setPrivateCompass(Compass, CompassState, compass);
}

/**
    Sets the compass state.  The compass reading can be empty or have a number (valid)
  */
void cwShot::setCompassState(cwCompassStates::State state)
{
    setPrivateCompassState(CompassState, state);
}

/// Sets the back sight compass for the shot
void cwShot::setBackCompass(QString backCompass) {
    setValueWithString(cwCompassValidator(), BackCompass, (int&)BackCompassState, backCompass);
}

void cwShot::setBackCompass(double backCompass)
{
    setPrivateCompass(BackCompass, BackCompassState, backCompass);
}

/**
    Sets the back compass state.  The back compass reading can be empty or have a number (valid)
  */
void cwShot::setBackCompassState(cwCompassStates::State state)
{
    setPrivateCompassState(BackCompassState, state);
}

void cwShot::setClino(QString clino) {
    setClinoValueWithString(Clino, (int&)ClinoState, clino);
}

void cwShot::setClino(double clino)
{
    setPrivateClino(Clino, ClinoState, clino);
}

void cwShot::setClinoState(cwClinoStates::State state)
{
    setPrivateClinoState(ClinoState, state);
}

void cwShot::setBackClino(QString backClino) {
    setClinoValueWithString(BackClino, (int&)BackClinoState, backClino);
}

void cwShot::setBackClino(double backClino)
{
    setPrivateClino(BackClino, BackClinoState, backClino);
}

void cwShot::setBackClinoState(cwClinoStates::State state)
{
    setPrivateClinoState(BackClinoState, state);
}

/**
  Sets the parent chunk
  */
void cwShot::setParentChunk(cwSurveyChunk* parentChunk) {
    ParentChunk = parentChunk;
}

/**
  \brief The parent chunk that this shot is connected to
  */
cwSurveyChunk* cwShot::parentChunk() const {
    return ParentChunk;
}

/**
  \brief The to station of this shot
  */
cwStationReference cwShot::toStation() const {
    cwSurveyChunk* chunk = parentChunk();
    if(chunk != NULL) {
        return chunk->toFromStations(this).second;
    }
    return cwStationReference();
}

/**
  \brief The from station of these shot
  */
cwStationReference cwShot::fromStation() const {
    cwSurveyChunk* chunk = parentChunk();
    if(chunk != NULL) {
        return chunk->toFromStations(this).first;
    }
    return cwStationReference();
}

/**
  \brief Sets the value with a new value, if valid
  */
cwShot::ValidState cwShot::setValueWithString(const cwValidator &validator, double &memberData, int& memberState, QString newValue)
{
    if(newValue.isEmpty()) {
        memberState = cwDistanceStates::Empty;
        return ValidEmpty;
    }

    if(validator.validate(newValue) == QValidator::Acceptable) {
        bool okay;
        double tempValue = newValue.toDouble(&okay);

        if(okay) {
            memberData = tempValue;
            memberState = cwDistanceStates::Valid;
            return ValidDouble;
        } else {
            return ValidString;
        }
    }

    return Invalid;
}

/**
  Sets the value of clino member data, if valid
  */
void cwShot::setClinoValueWithString(double &memberData, int &memberState, QString newValue)
{
    ValidState state = setValueWithString(cwClinoValidator(), memberData, memberState, newValue);
     if(state == ValidString) {
         if(newValue.compare("down", Qt::CaseInsensitive) == 0) {
            memberState = cwClinoStates::Down;
         } else if(newValue.compare("up", Qt::CaseInsensitive) == 0) {
            memberState = cwClinoStates::Up;
         }
     }
}

/**
  This is a convenance function to set the member for the back and front site compass
  */
void cwShot::setPrivateCompass(double &memberData, cwCompassStates::State &state, double value) {
    if(cwCompassValidator::check(value)) {
        memberData = value;
        state = cwCompassStates::Valid;
    }
}

/**
  This is a convenance function to set the member for the back and front site clino
  */
void cwShot::setPrivateClino(double &memberData, cwClinoStates::State &state, double value) {
    if(cwClinoValidator::check(value)) {
        memberData = value;
        state = cwClinoStates::Valid;
    }
}

/**
  This updates the memeberState.  This make sure newState is correct, for the compass.

  This function uses a switch statement because users can send typecast inergers into this.  We
  need to make sure all is well.
  */
void cwShot::setPrivateCompassState(cwCompassStates::State &memberState, cwCompassStates::State newState)
{
    //We need this switch statement because serialization class (or any other class) could send in interger
    switch(newState) {
    case cwCompassStates::Empty:
        memberState = cwCompassStates::Empty;
        break;
    case cwCompassStates::Valid:
        memberState = cwCompassStates::Valid;
        break;
    default:
        memberState = cwCompassStates::Empty;
        break;
    }
}

/**
  This updates the memeberState.  This make sure newState is correct, for the clino.

  This function uses a switch statement because users can send typecast inergers into this.  We
  need to make sure all is well.
  */
void cwShot::setPrivateClinoState(cwClinoStates::State &memberState, cwClinoStates::State newState)
{
    //We need this switch statement because serialization class (or any other class) could send in interger
    switch(newState) {
    case cwClinoStates::Empty:
        memberState = cwClinoStates::Empty;
        break;
    case cwClinoStates::Valid:
        memberState = cwClinoStates::Valid;
        break;
    case cwClinoStates::Down:
        memberState = cwClinoStates::Down;
        break;
    case cwClinoStates::Up:
        memberState = cwClinoStates::Up;
        break;
    default:
        memberState = cwClinoStates::Empty;
        break;
    }
}
