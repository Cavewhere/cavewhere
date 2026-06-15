/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSYMBOLOGYPALETTEMANAGER_H
#define CWSYMBOLOGYPALETTEMANAGER_H

//Qt includes
#include <QList>
#include <QObject>
#include <QString>
#include <QUuid>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwError.h"

class cwSymbologyPalette;
class cwErrorModel;

// Owns the set of installed palettes. The built-in seed palette is always
// present. The on-disk palette directory is scanned for subdirectories
// containing a palette.json; a palette's identity is its Palette.id, not its
// directory name, so duplicate ids resolve first-scanned-wins with a one-shot
// warning naming both directories.
//
// App code uses the singleton (initialize()/instance()). Tests construct their
// own instance pointed at a temporary directory, so concurrent test processes
// don't share state.
class CAVEWHERE_LIB_EXPORT cwSymbologyPaletteManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cwErrorModel* errorModel READ errorModel CONSTANT)

public:
    explicit cwSymbologyPaletteManager(QObject *parent = nullptr);

    static void initialize();
    static cwSymbologyPaletteManager *instance();

    // Default: <AppDataLocation>/palettes. Setting it rescans.
    QString paletteDirectory() const { return m_paletteDirectory; }
    void setPaletteDirectory(const QString &directory);

    // The model of non-fatal load problems from the most recent reload(): failed
    // palettes, duplicate-id skips, forward-incompatible palettes, and rule-stack
    // warnings. Owned by the manager and refreshed on each reload(); bind to it
    // for UI display. fatalCount stays 0 — fatal problems fail the palette load
    // outright rather than landing here.
    cwErrorModel *errorModel() const { return m_errorModel; }

    // Rescan the palette directory. The seed is always re-added first.
    void reload();

    // Seed first, then installed palettes in scan order.
    QList<cwSymbologyPalette *> palettes() const { return m_palettes; }
    cwSymbologyPalette *seedPalette() const { return m_seed; }
    cwSymbologyPalette *paletteById(const QUuid &id) const;

    static QString defaultPaletteDirectory();

signals:
    void palettesChanged();

private:
    void clearInstalled();

    // Record a non-fatal reload problem into the owned error model (de-duped,
    // qWarning'd). The QString overload is a Warning convenience; the cwError
    // overload preserves a typed error's severity and SymbologyErrorCode (the
    // editor keys off the code).
    void reportLoadProblem(const cwError &error);
    void reportLoadProblem(const QString &message);

    static cwSymbologyPaletteManager *Singleton;

    QString m_paletteDirectory;
    cwSymbologyPalette *m_seed = nullptr;
    QList<cwSymbologyPalette *> m_palettes;
    cwErrorModel *m_errorModel = nullptr;
};

#endif // CWSYMBOLOGYPALETTEMANAGER_H
