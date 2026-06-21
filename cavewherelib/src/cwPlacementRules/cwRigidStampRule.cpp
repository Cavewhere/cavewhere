/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwRigidStampRule.h"

QString cwRigidStampRule::displayName() const
{
    return QStringLiteral("Rigid stamp");
}

QString cwRigidStampRule::description() const
{
    return QStringLiteral("Draws each glyph straight and undistorted at its placed "
                          "spot.");
}
