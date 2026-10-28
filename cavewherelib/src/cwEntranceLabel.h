/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWENTRANCELABEL_H
#define CWENTRANCELABEL_H

//Qt includes
#include <QString>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwUnits.h"

/**
 * @brief The stats line an entrance marker draws under its name in the 3D view.
 *
 * "→<length>  ↓<depth>" in @a unitSystem, the length in the
 * large unit (km / mi) at one decimal once it reaches one large unit and in the
 * small unit (m / ft) at no decimals below that, the depth always in the small
 * unit at no decimals.
 */
class CAVEWHERE_LIB_EXPORT cwEntranceLabel
{
public:
    static QString statsText(double lengthMeters,
                             double depthMeters,
                             cwUnits::UnitSystem unitSystem);
};

#endif // CWENTRANCELABEL_H
