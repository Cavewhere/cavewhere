/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWANGLEINPUT_H
#define CWANGLEINPUT_H

#include "cwUnitValueInput.h"
#include "cwCompassValidator.h"
#include "cwUnits.h"

typedef cwUnitValueInput<cwUnits::AngleUnit, cwCompassValidator> cwAngleInput;

#endif // CWANGLEINPUT_H
