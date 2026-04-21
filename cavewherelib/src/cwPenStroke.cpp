/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPenStroke.h"

QRectF cwPenStroke::boundingBox() const
{
    return cwBoundingBoxOf(points, [](const cwPenPoint &p) { return p.position; });
}
