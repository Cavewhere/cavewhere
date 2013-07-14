/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwShot.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwDistanceValidator.h"
#include "cwCompassValidator.h"
#include "cwClinoValidator.h"
#include "cwValidator.h"


cwShot::cwShot() :
    Data(new PrivateData())
{    }

cwShot::cwShot(QString distance,
               QString compass,
               QString backCompass,
               QString clino,
               QString backClino) :
    Data(new PrivateData())
{

    setDistance(distance);
    setCompass(compass);
    setBackCompass(backCompass);
    setClino(clino);
    setBackClino(backClino);
}

cwShot::cwShot(const cwShot &shot) :
    Data(shot.Data)
{
}

cwShot::PrivateData::PrivateData() :
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
    IncludeDistance(true)
{

}

void cwShot::setDistance(QString distance) {
    cwDistanceValidator validator;
    setValueWithString(validator, Data->Distance, (int&)Data->DistanceState, distance);
}

void cwShot::setDistance(double distance) {
    if(cwDistanceValidator::check(distance)) {
        Data->Distance = distance;
        Data->DistanceState = cwDistanceStates::Valid;
    }
}

void cwShot::setDistanceState(cwDistanceStates::State state)
{
    //We need this switch statement because serialization class (or any other class) could send in interger
    switch(state) {
    case cwDistanceStates::Empty:
        Data->DistanceState = cwDistanceStates::Empty;
        break;
    case cwDistanceStates::Valid:
        Data->DistanceState = cwDistanceStates::Valid;
        break;
    default:
        Data->DistanceState = cwDistanceStates::Empty;
        break;
    }
}

void cwShot::setCompass(QString compass) {
    cwCompassValidator validator;
    setValueWithString(validator, Data->Compass, (int&)Data->CompassState, compass);
}

/// Sets the front sight compass for the shot
void cwShot::setCompass(double compass)
{
    setPrivateCompass(Data->Compass, Data->CompassState, compass);
}

/**
    Sets the compass state.  The compass reading can be empty or have a number (valid)
  */
void cwShot::setCompassState(cwCompassStates::State state)
{
    setPrivateCompassState(Data->CompassState, state);
}

/// Sets the back sight compass for the shot
void cwShot::setBackCompass(QString backCompass) {
    cwCompassValidator validator;
    setValueWithString(validator, Data->BackCompass, (int&)Data->BackCompassState, backCompass);
}

void cwShot::setBackCompass(double backCompass)
{
    setPrivateCompass(Data->BackCompass, Data->BackCompassState, backCompass);
}

/**
    Sets the back compass state.  The back compass reading can be empty or have a number (valid)
  */
void cwShot::setBackCompassState(cwCompassStates::State state)
{
    setPrivateCompassState(Data->BackCompassState, state);
}

void cwShot::setClino(QString clino) {
    setClinoValueWithString(Data->Clino, (int&)Data->ClinoState, clino);
}

void cwShot::setClino(double clino)
{
    setPrivateClino(Data->Clino, Data->ClinoState, clino);
}

void cwShot::setClinoState(cwClinoStates::State state)
{
    setPrivateClinoState(Data->ClinoState, state);
}

void cwShot::setBackClino(QString backClino) {
    setClinoValueWithString(Data->BackClino, (int&)Data->BackClinoState, backClino);
}

void cwShot::setBackClino(double backClino)
{
    setPrivateClino(Data->BackClino, Data->BackClinoState, backClino);
}

void cwShot::setBackClinoState(cwClinoStates::State state)
{
    setPrivateClinoState(Data->BackClinoState, state);
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
    cwClinoValidator validator;
    ValidState state = setValueWithString(validator, memberData, memberState, newValue);
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

