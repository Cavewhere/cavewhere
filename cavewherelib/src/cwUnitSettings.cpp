//Our includes
#include "cwUnitSettings.h"

//Qt includes
#include <QSettings>
#include <QCoreApplication>

cwUnitSettings* cwUnitSettings::Settings = nullptr;

cwUnitSettings::cwUnitSettings(QObject* parent) :
    QObject(parent)
{
    QSettings settings;
    m_unitSystem = static_cast<cwUnits::UnitSystem>(
        settings.value(unitSystemKey(), m_unitSystem).toInt());
}

void cwUnitSettings::setUnitSystem(cwUnits::UnitSystem system)
{
    if (m_unitSystem == system) {
        return;
    }
    m_unitSystem = system;
    QSettings settings;
    settings.setValue(unitSystemKey(), static_cast<int>(system));
    emit unitSystemChanged();
}

cwUnitSettings* cwUnitSettings::instance()
{
    return Settings;
}

void cwUnitSettings::initialize()
{
    if (Settings == nullptr) {
        Settings = new cwUnitSettings(QCoreApplication::instance());
    }
}
