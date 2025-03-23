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
#include "cwDistanceReading.h"


cwShot::cwShot() :
    Data(new PrivateData())
{    }

cwShot::cwShot(const QString& distance,
       const QString& compass, const QString& backCompass,
       const QString& clino, const QString& backClino) :
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
    IncludeDistance(true)
{

}


