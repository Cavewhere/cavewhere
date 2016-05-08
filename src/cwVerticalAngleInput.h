/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWVERTICALANGLEINPUT_H
#define CWVERTICALANGLEINPUT_H

#include "cwUnitValueInput.h"
#include "cwClinoValidator.h"
#include "cwUnits.h"

typedef cwUnitValueInput<cwUnits::VerticalAngleUnit, cwClinoValidator> cwVerticalAngleInput;

#endif // CWVERTICALANGLEINPUT_H
