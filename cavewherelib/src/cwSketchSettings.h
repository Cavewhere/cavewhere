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

class CAVEWHERE_LIB_EXPORT cwSketchSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SketchSettings)
    QML_UNCREATABLE("SketchSettings is a cavewhere singleton and can't be created directly")

    Q_PROPERTY(bool filterPenHooks READ filterPenHooks WRITE setFilterPenHooks NOTIFY filterPenHooksChanged)

public:
    bool filterPenHooks() const { return FilterPenHooks; }
    void setFilterPenHooks(bool enabled);

    static cwSketchSettings* instance();
    static void initialize();

signals:
    void filterPenHooksChanged();

private:
    static cwSketchSettings* Settings;

    static QString filterPenHooksKey() { return QLatin1String("filterPenHooks"); }

    bool FilterPenHooks = false;

    explicit cwSketchSettings(QObject* parent = nullptr);
};

#endif // CWSKETCHSETTINGS_H
