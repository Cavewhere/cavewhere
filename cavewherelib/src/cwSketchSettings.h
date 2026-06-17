#ifndef CWSKETCHSETTINGS_H
#define CWSKETCHSETTINGS_H

//Qt includes
#include <QObject>
#include <QQmlEngine>
#include <QUuid>

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
    Q_PROPERTY(QUuid defaultPaletteId READ defaultPaletteId WRITE setDefaultPaletteId NOTIFY defaultPaletteIdChanged)

public:
    bool autoIconUpdates() const { return AutoIconUpdates; }
    void setAutoIconUpdates(bool enabled);

    int  idleIntervalMs() const { return IdleIntervalMs; }
    void setIdleIntervalMs(int ms);

    // App-wide default symbology palette — the third level of the per-sketch
    // resolver chain (sketch → region → settings → shipped default). Null means
    // "fall through to the shipped default." Persisted via QSettings, so it's a
    // remembered preference for new sketches, not part of any project file.
    QUuid defaultPaletteId() const { return DefaultPaletteId; }
    void setDefaultPaletteId(const QUuid& id);

    Q_INVOKABLE void resetToDefaults();

    static cwSketchSettings* instance();
    static void initialize();

signals:
    void autoIconUpdatesChanged();
    void idleIntervalMsChanged();
    void defaultPaletteIdChanged();

private:
    explicit cwSketchSettings(QObject* parent = nullptr);

    static cwSketchSettings* Settings;

    // Defaults derived from cwRootData::isMobileBuild() at construction time.
    bool AutoIconUpdates;
    int  IdleIntervalMs;
    QUuid DefaultPaletteId;
};

#endif // CWSKETCHSETTINGS_H
