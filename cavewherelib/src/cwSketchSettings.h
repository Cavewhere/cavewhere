#ifndef CWSKETCHSETTINGS_H
#define CWSKETCHSETTINGS_H

//Qt includes
#include <QObject>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"

// Tunables for cwSketchManager's async icon pipeline. Kept as a singleton so
// the defaults live in one place and can be tweaked from QML / tests without
// touching cwRootData wiring.
class CAVEWHERE_LIB_EXPORT cwSketchSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SketchSettings)
    QML_UNCREATABLE("SketchSettings is a cavewhere singleton and can't be created directly")

    Q_PROPERTY(bool autoIconUpdates READ autoIconUpdates WRITE setAutoIconUpdates NOTIFY autoIconUpdatesChanged)
    Q_PROPERTY(int  idleIntervalMs  READ idleIntervalMs  WRITE setIdleIntervalMs  NOTIFY idleIntervalMsChanged)

public:
    bool autoIconUpdates() const { return AutoIconUpdates; }
    void setAutoIconUpdates(bool enabled);

    int  idleIntervalMs() const { return IdleIntervalMs; }
    void setIdleIntervalMs(int ms);

    Q_INVOKABLE void resetToDefaults();

    static cwSketchSettings* instance();
    static void initialize();

signals:
    void autoIconUpdatesChanged();
    void idleIntervalMsChanged();

private:
    explicit cwSketchSettings(QObject* parent = nullptr);

    static cwSketchSettings* Settings;

    // Defaults derived from cwRootData::isMobileBuild() at construction time.
    bool AutoIconUpdates;
    int  IdleIntervalMs;
};

#endif // CWSKETCHSETTINGS_H
