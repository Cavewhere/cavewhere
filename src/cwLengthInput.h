/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWDISTANCE_H
#define CWDISTANCE_H

//Qt includse
//#include <QValidator>
//#include <QRegularExpression>

//Our includes
#include "cwUnits.h"
//#include "cwGlobals.h"
#include "cwUnitValueInput.h"
#include "cwDistanceValidator.h"

typedef cwUnitValueInput<cwUnits::LengthUnit, cwDistanceValidator> cwLengthInput;

#endif // CWDISTANCE_H
