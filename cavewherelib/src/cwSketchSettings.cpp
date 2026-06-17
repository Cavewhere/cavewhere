//Our includes
#include "cwSketchSettings.h"
#include "cwRootData.h"

//Qt includes
#include <QCoreApplication>
#include <QSettings>

namespace {
QString autoIconUpdatesKey() { return QStringLiteral("sketch/autoIconUpdates"); }
QString idleIntervalMsKey()  { return QStringLiteral("sketch/idleIntervalMs"); }
QString defaultPaletteIdKey() { return QStringLiteral("sketch/defaultPaletteId"); }

// Rendering thumbnails while the stylus is active lags sketching and eats
// battery. Desktop defaults to live-updating; mobile defers to navigation
// flushes (see cwSketchManager::flushIconIfDirty).
bool defaultAutoIconUpdates() { return !cwRootData::isMobileBuild(); }
int  defaultIdleIntervalMs()  { return cwRootData::isMobileBuild() ? 5000 : 3000; }
}

cwSketchSettings* cwSketchSettings::Settings = nullptr;

cwSketchSettings::cwSketchSettings(QObject* parent) :
    QObject(parent)
{
    QSettings settings;
    AutoIconUpdates = settings.value(autoIconUpdatesKey(), defaultAutoIconUpdates()).toBool();
    IdleIntervalMs  = qMax(0, settings.value(idleIntervalMsKey(), defaultIdleIntervalMs()).toInt());
    DefaultPaletteId = QUuid(settings.value(defaultPaletteIdKey()).toString());
}

void cwSketchSettings::setDefaultPaletteId(const QUuid& id)
{
    if (DefaultPaletteId == id) {
        return;
    }
    DefaultPaletteId = id;
    QSettings settings;
    if (id.isNull()) {
        settings.remove(defaultPaletteIdKey());
    } else {
        settings.setValue(defaultPaletteIdKey(), id.toString(QUuid::WithoutBraces));
    }
    emit defaultPaletteIdChanged();
}

void cwSketchSettings::resetToDefaults()
{
    setAutoIconUpdates(defaultAutoIconUpdates());
    setIdleIntervalMs(defaultIdleIntervalMs());
    setDefaultPaletteId(QUuid());
}

void cwSketchSettings::setAutoIconUpdates(bool enabled)
{
    if (AutoIconUpdates == enabled) {
        return;
    }
    AutoIconUpdates = enabled;
    QSettings settings;
    settings.setValue(autoIconUpdatesKey(), enabled);
    emit autoIconUpdatesChanged();
}

void cwSketchSettings::setIdleIntervalMs(int ms)
{
    if (ms < 0) {
        ms = 0;
    }
    if (IdleIntervalMs == ms) {
        return;
    }
    IdleIntervalMs = ms;
    QSettings settings;
    settings.setValue(idleIntervalMsKey(), ms);
    emit idleIntervalMsChanged();
}

cwSketchSettings* cwSketchSettings::instance()
{
    return Settings;
}

void cwSketchSettings::initialize()
{
    if (Settings == nullptr) {
        Settings = new cwSketchSettings(QCoreApplication::instance());
    }
}
