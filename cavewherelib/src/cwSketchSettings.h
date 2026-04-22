/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCHSETTINGS_H
#define CWSKETCHSETTINGS_H

//Qt includes
#include <QObject>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"
#include "cwPenStrokeFilter.h"

class CAVEWHERE_LIB_EXPORT cwSketchSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SketchSettings)
    QML_UNCREATABLE("SketchSettings is a cavewhere singleton and can't be created directly")

    Q_PROPERTY(bool filterPenHooks READ filterPenHooks WRITE setFilterPenHooks NOTIFY filterPenHooksChanged)
    Q_PROPERTY(double maxHookLengthMm READ maxHookLengthMm WRITE setMaxHookLengthMm NOTIFY maxHookLengthMmChanged)
    Q_PROPERTY(double maxHookFraction READ maxHookFraction WRITE setMaxHookFraction NOTIFY maxHookFractionChanged)

public:
    bool filterPenHooks() const { return FilterPenHooks; }
    void setFilterPenHooks(bool enabled);

    // Cap expressed in physical millimetres on the iPad surface.
    // The call site converts to world meters using the sketch's
    // current scale and zoom (see penStrokeFilterParams).
    double maxHookLengthMm() const { return MaxHookLengthMm; }
    void setMaxHookLengthMm(double millimetres);

    double maxHookFraction() const { return MaxHookFraction; }
    void setMaxHookFraction(double fraction);

    // Builds filter params for a given view: `scaleRatio` is the
    // paper:world ratio (e.g. 0.004 for 1:250, from cwScale::scale()),
    // `zoom` is the view's current zoom multiplier. Converts the
    // user-facing physical millimetres into a world-meter cap via
    //   worldM = physicalMm / (zoom * scaleRatio * 1000).
    cwPenStrokeFilter::Params penStrokeFilterParams(double scaleRatio,
                                                    double zoom) const;

    static cwSketchSettings* instance();
    static void initialize();

signals:
    void filterPenHooksChanged();
    void maxHookLengthMmChanged();
    void maxHookFractionChanged();

private:
    static cwSketchSettings* Settings;

    static QString filterPenHooksKey() { return QLatin1String("filterPenHooks"); }
    static QString maxHookLengthMmKey() { return QLatin1String("maxHookLengthMm"); }
    static QString maxHookFractionKey() { return QLatin1String("maxHookFraction"); }

    bool FilterPenHooks = false;
    // 2 mm covers typical Apple Pencil press-down slip (~0.5–1.5 mm) with
    // headroom. Zoom-independent because we convert to world meters at
    // call time.
    double MaxHookLengthMm = 2.0;
    double MaxHookFraction = cwPenStrokeFilter::Params{}.maxHookFraction;

    explicit cwSketchSettings(QObject* parent = nullptr);
};

#endif // CWSKETCHSETTINGS_H
