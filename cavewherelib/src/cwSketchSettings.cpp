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
    FilterPenHooks  = settings.value(filterPenHooksKey(), FilterPenHooks).toBool();
    MaxHookLengthMm = settings.value(maxHookLengthMmKey(), MaxHookLengthMm).toDouble();
    MaxHookFraction = settings.value(maxHookFractionKey(), MaxHookFraction).toDouble();
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

void cwSketchSettings::setMaxHookLengthMm(double millimetres)
{
    if (MaxHookLengthMm == millimetres) {
        return;
    }
    QSettings settings;
    settings.setValue(maxHookLengthMmKey(), millimetres);
    MaxHookLengthMm = millimetres;
    emit maxHookLengthMmChanged();
}

void cwSketchSettings::setMaxHookFraction(double fraction)
{
    if (MaxHookFraction == fraction) {
        return;
    }
    QSettings settings;
    settings.setValue(maxHookFractionKey(), fraction);
    MaxHookFraction = fraction;
    emit maxHookFractionChanged();
}

cwPenStrokeFilter::Params cwSketchSettings::penStrokeFilterParams(double scaleRatio,
                                                                  double zoom) const
{
    cwPenStrokeFilter::Params p;
    const double safeScale = scaleRatio > 0.0 ? scaleRatio : 0.004;  // 1:250 fallback
    const double safeZoom  = zoom       > 0.0 ? zoom       : 1.0;
    p.maxHookLength   = (MaxHookLengthMm / 1000.0) / (safeZoom * safeScale);
    p.maxHookFraction = MaxHookFraction;
    return p;
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
