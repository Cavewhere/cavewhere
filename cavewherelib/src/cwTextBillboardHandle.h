/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTEXTBILLBOARDHANDLE_H
#define CWTEXTBILLBOARDHANDLE_H

// RAII owner of one station label in the shared text layer; same contract as
// cwBillboardHandle (see cwLayerBillboardHandle).

//Our includes
#include "cwBillboardHandle.h"
#include "cwRenderTextBillboards.h"

using cwTextBillboardHandle = cwLayerBillboardHandle<cwRenderTextBillboards,
                                                     cwRenderTextBillboards::TextBillboard>;

#endif // CWTEXTBILLBOARDHANDLE_H
