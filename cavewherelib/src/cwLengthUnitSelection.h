/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLENGTHUNITSELECTION_H
#define CWLENGTHUNITSELECTION_H

//Our includes
#include "cwUnits.h"
#include "cwGlobals.h"

//Qt includes
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

/**
 * A selectable length unit shared across several canonical-metre values — the
 * one display unit a readout reports all its lengths in (e.g. the measurement
 * tool's distance / horizontal / by-axis components).
 *
 * Unlike cwLength / cwUnitValue, which couple a single value to its unit, this
 * holds no value: it is just the chosen unit plus the conversion and naming a
 * view needs. It offers a curated subset of cwUnits::LengthUnit (metres,
 * kilometres, feet, miles) so a UnitCombo can bind `names` as its model and
 * drive the choice by `index`. When `settingsKey` is set, the choice is loaded
 * from and saved to that QSettings key, so it persists across sessions.
 *
 * This is the single source of truth for the curated length-unit set;
 * UnitDefaults.lengthModel derives its model from unitNames().
 */
class CAVEWHERE_LIB_EXPORT cwLengthUnitSelection : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LengthUnitSelection)

    Q_PROPERTY(cwUnits::LengthUnit unit READ unit WRITE setUnit NOTIFY unitChanged)
    // Index of the selected unit within names(), for an index-based selector
    // (UnitCombo) that stays enum-agnostic.
    Q_PROPERTY(int index READ index WRITE setIndex NOTIFY unitChanged)
    // The curated set's unit names, in menu order — the selector's model. Fixed
    // for the object's lifetime, hence CONSTANT.
    Q_PROPERTY(QStringList names READ names CONSTANT)
    // The suffix of the currently selected unit (e.g. "m", "ft").
    Q_PROPERTY(QString name READ name NOTIFY unitChanged)
    // When non-empty, the selection is loaded from and saved to this QSettings key.
    Q_PROPERTY(QString settingsKey READ settingsKey WRITE setSettingsKey NOTIFY settingsKeyChanged)

public:
    explicit cwLengthUnitSelection(QObject* parent = nullptr);

    //! The curated set of selectable units, in menu order: m, km, ft, mi.
    static QList<cwUnits::LengthUnit> curatedUnits();
    //! The curated set's names, in menu order (single source for UnitDefaults).
    static QStringList unitNames();

    cwUnits::LengthUnit unit() const { return m_unit; }
    void setUnit(cwUnits::LengthUnit unit);

    int index() const;
    void setIndex(int index);

    QStringList names() const { return unitNames(); }
    QString name() const;

    QString settingsKey() const { return m_settingsKey; }
    void setSettingsKey(const QString& key);

    //! The unit used when nothing is persisted yet — the owner sets it from the
    //! project's unit system so a fresh selection follows Metric/Imperial. A
    //! persisted choice always wins, so this never overrides the user.
    void setDefaultUnit(cwUnits::LengthUnit unit);

    //! The magnitude converted from canonical metres into the selected unit.
    Q_INVOKABLE double fromMeters(double meters) const;
    //! The inverse: a magnitude in the selected unit back to canonical metres.
    Q_INVOKABLE double toMeters(double value) const;

    //! A canonical-metre length rendered in the selected unit, at display
    //! precision, with the unit suffix (e.g. "196.85 ft") — the QML-facing entry
    //! to cwUnits::formatLength, so the on-screen panel, the on-line chip, and
    //! the clipboard share one format. When \a signedValue is true, a positive
    //! value gets an explicit '+' (the by-axis direction); a value that rounds to
    //! zero carries no sign.
    Q_INVOKABLE QString format(double meters, bool signedValue = false) const;

signals:
    void unitChanged();
    void settingsKeyChanged();

private:
    //! Clamp a unit to the curated set, falling back to Meters — so a future or
    //! corrupt settings value can't wedge the selection on an unlisted unit.
    static cwUnits::LengthUnit coerceToSet(cwUnits::LengthUnit unit);
    //! Set the unit without persisting — the path for seeds (setDefaultUnit) and
    //! loads. Only setUnit() writes the settings key, so a stored value always
    //! marks a deliberate user choice.
    void applyUnit(cwUnits::LengthUnit unit);
    void loadFromSettings();

    cwUnits::LengthUnit m_unit = cwUnits::Meters;
    cwUnits::LengthUnit m_defaultUnit = cwUnits::Meters;
    QString m_settingsKey;
};

#endif // CWLENGTHUNITSELECTION_H
