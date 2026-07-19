/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLengthUnitSelection.h"

//Qt includes
#include <QSettings>

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
    // setUnit is the explicit-choice path (the popup / an index pick): it both
    // applies the unit and persists it, so a stored value marks a deliberate
    // user choice. Seeds and loads go through applyUnit() and never persist.
    const cwUnits::LengthUnit coerced = coerceToSet(unit);
    if (m_unit == coerced) {
        return;
    }
    m_unit = coerced;
    if (!m_settingsKey.isEmpty()) {
        QSettings settings;
        settings.setValue(m_settingsKey, int(m_unit));
    }
    emit unitChanged();
}

void cwLengthUnitSelection::applyUnit(cwUnits::LengthUnit unit)
{
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

void cwLengthUnitSelection::setSettingsKey(const QString& key)
{
    if (m_settingsKey == key) {
        return;
    }
    m_settingsKey = key;
    emit settingsKeyChanged();
    loadFromSettings();
}

void cwLengthUnitSelection::setDefaultUnit(cwUnits::LengthUnit unit)
{
    m_defaultUnit = coerceToSet(unit);
    // A persisted value is a deliberate user choice and always wins. Otherwise
    // apply the default *without* persisting, so the selection stays "unset" and
    // keeps following later project changes until the user explicitly picks.
    if (!m_settingsKey.isEmpty()) {
        QSettings settings;
        if (settings.contains(m_settingsKey)) {
            return;
        }
    }
    applyUnit(m_defaultUnit);
}

void cwLengthUnitSelection::loadFromSettings()
{
    if (m_settingsKey.isEmpty()) {
        return;
    }
    // A stored value is the user's choice; absent one, fall back to the default.
    // Neither path persists (applyUnit) — only an explicit setUnit writes the key.
    QSettings settings;
    if (settings.contains(m_settingsKey)) {
        // applyUnit clamps an unlisted/corrupt value to metres.
        applyUnit(static_cast<cwUnits::LengthUnit>(settings.value(m_settingsKey).toInt()));
    } else {
        applyUnit(m_defaultUnit);
    }
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
