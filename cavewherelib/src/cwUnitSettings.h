#ifndef CWUNITSETTINGS_H
#define CWUNITSETTINGS_H

//Qt includes
#include <QObject>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"
#include "cwUnits.h"

//! App-level default unit system. Seeds the project-level unitSystem stored in
//! cwCavingRegion (the source of truth once a project exists) and is the
//! fallback when no project is open. QSettings-backed, mirroring cwFontSettings.
class CAVEWHERE_LIB_EXPORT cwUnitSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(UnitSettings)
    QML_UNCREATABLE("UnitSettings is a cavewhere singleton and can't be created directly")

    Q_PROPERTY(cwUnits::UnitSystem unitSystem READ unitSystem WRITE setUnitSystem NOTIFY unitSystemChanged)

public:
    cwUnits::UnitSystem unitSystem() const;
    void setUnitSystem(cwUnits::UnitSystem system);

    static cwUnitSettings* instance();
    static void initialize();

signals:
    void unitSystemChanged();

private:
    explicit cwUnitSettings(QObject* parent = nullptr);

    static cwUnitSettings* Settings;

    static QString unitSystemKey() { return QStringLiteral("unitSystem"); }

    cwUnits::UnitSystem m_unitSystem = cwUnits::Metric;
};

inline cwUnits::UnitSystem cwUnitSettings::unitSystem() const { return m_unitSystem; }

#endif // CWUNITSETTINGS_H
