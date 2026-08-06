/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLengthUnitSelection.h"

//Std includes
#include <array>

namespace {
    // The curated length units offered to the user, in menu order (m, km, ft,
    // mi). Single source of truth for the selector's model.
    constexpr std::array<cwUnits::LengthUnit, 4> kCuratedUnits = {
        cwUnits::Meters, cwUnits::Kilometers, cwUnits::Feet, cwUnits::Miles
    };
}

cwLengthUnitSelection::cwLengthUnitSelection(QObject* parent) :
    QObject(parent)
{
}

QList<cwUnits::LengthUnit> cwLengthUnitSelection::curatedUnits()
{
    return QList<cwUnits::LengthUnit>(kCuratedUnits.begin(), kCuratedUnits.end());
}

QStringList cwLengthUnitSelection::unitNames()
{
    QStringList names;
    names.reserve(int(kCuratedUnits.size()));
    for (cwUnits::LengthUnit unit : kCuratedUnits) {
        names.append(cwUnits::unitName(unit));
    }
    return names;
}

cwUnits::LengthUnit cwLengthUnitSelection::coerceToSet(cwUnits::LengthUnit unit)
{
    for (cwUnits::LengthUnit candidate : kCuratedUnits) {
        if (candidate == unit) {
            return unit;
        }
    }
    return cwUnits::Meters;
}

void cwLengthUnitSelection::setUnit(cwUnits::LengthUnit unit)
{
    // The one place the current unit changes — both the project-driven seed and
    // the popup pick land here. Nothing is persisted: the selection is in-memory,
    // so it always re-follows the project's unit system on the next change.
    const cwUnits::LengthUnit coerced = coerceToSet(unit);
    if (m_unit == coerced) {
        return;
    }
    m_unit = coerced;
    emit unitChanged();
}

int cwLengthUnitSelection::index() const
{
    for (int i = 0; i < int(kCuratedUnits.size()); ++i) {
        if (kCuratedUnits[i] == m_unit) {
            return i;
        }
    }
    return 0;
}

void cwLengthUnitSelection::setIndex(int index)
{
    if (index < 0 || index >= int(kCuratedUnits.size())) {
        return;
    }
    setUnit(kCuratedUnits[index]);
}

QString cwLengthUnitSelection::name() const
{
    return cwUnits::unitName(m_unit);
}

double cwLengthUnitSelection::fromMeters(double meters) const
{
    return cwUnits::convert(meters, cwUnits::Meters, m_unit);
}

double cwLengthUnitSelection::toMeters(double value) const
{
    return cwUnits::convert(value, m_unit, cwUnits::Meters);
}

QString cwLengthUnitSelection::format(double meters, bool signedValue) const
{
    return cwUnits::formatLength(meters, m_unit, signedValue);
}
