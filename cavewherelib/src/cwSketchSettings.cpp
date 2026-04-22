/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwSketchSettings.h"

//Qt includes
#include <QCoreApplication>
#include <QSettings>

cwSketchSettings* cwSketchSettings::Settings = nullptr;

cwSketchSettings::cwSketchSettings(QObject *parent) :
    QObject(parent)
{
    QSettings settings;
    FilterPenHooks = settings.value(filterPenHooksKey(), FilterPenHooks).toBool();
}

void cwSketchSettings::setFilterPenHooks(bool enabled)
{
    if (FilterPenHooks == enabled) {
        return;
    }
    QSettings settings;
    settings.setValue(filterPenHooksKey(), enabled);
    FilterPenHooks = enabled;
    emit filterPenHooksChanged();
}

cwSketchSettings *cwSketchSettings::instance()
{
    return Settings;
}

void cwSketchSettings::initialize()
{
    if (Settings == nullptr) {
        Settings = new cwSketchSettings(QCoreApplication::instance());
    }
}
