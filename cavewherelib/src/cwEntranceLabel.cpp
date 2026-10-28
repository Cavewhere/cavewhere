/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwEntranceLabel.h"

namespace {
constexpr int kLargeUnitDecimals = 1;
constexpr int kSmallUnitDecimals = 0;
const QString kPartSeparator = QStringLiteral("  ");
const QString kLengthArrow = QStringLiteral("→");
const QString kDepthArrow = QStringLiteral("↓");

QString formatMeters(double meters, cwUnits::LengthUnit unit, int decimals)
{
    return QString::number(cwUnits::convert(meters, cwUnits::Meters, unit), 'f', decimals)
           + QStringLiteral(" ") + cwUnits::unitName(unit);
}
}

QString cwEntranceLabel::statsText(cwFixStation::Kind kind,
                                   double lengthMeters,
                                   double depthMeters,
                                   cwUnits::UnitSystem unitSystem)
{
    if(kind != cwFixStation::Entrance) {
        return QString();
    }

    const cwUnits::LengthUnit lengthUnit = cwUnits::magnitudeUnit(lengthMeters, unitSystem);
    const int lengthDecimals = lengthUnit == cwUnits::largeLengthUnit(unitSystem)
                                   ? kLargeUnitDecimals
                                   : kSmallUnitDecimals;

    return kLengthArrow + formatMeters(lengthMeters, lengthUnit, lengthDecimals)
           + kPartSeparator + kDepthArrow
           + formatMeters(depthMeters, cwUnits::depthUnit(unitSystem), kSmallUnitDecimals);
}
