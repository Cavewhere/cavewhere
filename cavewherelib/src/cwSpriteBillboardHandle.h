/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSPRITEBILLBOARDHANDLE_H
#define CWSPRITEBILLBOARDHANDLE_H

// RAII owner of one sprite instance in the shared sprite layer; same contract as
// cwBillboardHandle (see cwLayerBillboardHandle).

//Our includes
#include "cwBillboardHandle.h"
#include "cwRenderSpriteBillboards.h"

using cwSpriteBillboardHandle = cwLayerBillboardHandle<cwRenderSpriteBillboards,
                                                       cwRenderSpriteBillboards::SpriteBillboard>;

#endif // CWSPRITEBILLBOARDHANDLE_H
