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
#include <QStringList>
#include <QUuid>

//Our includes
#include "CaveWhereLibExport.h"

class cwSymbologyPalette;
class cwErrorListModel;

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

public:
    explicit cwSymbologyPaletteManager(QObject *parent = nullptr);

    static void initialize();
    static cwSymbologyPaletteManager *instance();

    // Default: <AppDataLocation>/palettes. Setting it rescans.
    QString paletteDirectory() const { return m_paletteDirectory; }
    void setPaletteDirectory(const QString &directory);

    // Where reload() posts non-fatal load problems (failed palettes, duplicate
    // ids, forward-incompatible palettes) as warnings, so the UI shows them.
    // Optional; nullptr disables posting. Setting it replays the most recent
    // reload()'s problems, covering the constructor reload that ran before the
    // app could wire a model. The manager never clears the model (it doesn't own
    // it) and de-dupes before appending.
    void setErrorModel(cwErrorListModel *model);

    // Rescan the palette directory. The seed is always re-added first.
    void reload();

    // Seed first, then installed palettes in scan order.
    QList<cwSymbologyPalette *> palettes() const { return m_palettes; }
    cwSymbologyPalette *seedPalette() const { return m_seed; }
    cwSymbologyPalette *paletteById(const QUuid &id) const;

    // Non-fatal problems from the most recent reload(): palettes that failed to
    // load and duplicate-id skips. Empty when every directory loaded cleanly.
    // Refreshed on each reload(); query it after palettesChanged().
    QStringList loadErrors() const { return m_loadErrors; }

    static QString defaultPaletteDirectory();

signals:
    void palettesChanged();

private:
    void clearInstalled();

    // Record a non-fatal reload problem: append to m_loadErrors, qWarning, and
    // post to the error model (de-duped) when one is wired.
    void reportLoadProblem(const QString &message);
    void postToErrorModel(const QString &message);

    static cwSymbologyPaletteManager *Singleton;

    QString m_paletteDirectory;
    cwSymbologyPalette *m_seed = nullptr;
    QList<cwSymbologyPalette *> m_palettes;
    QStringList m_loadErrors;
    cwErrorListModel *m_errorModel = nullptr;
};

#endif // CWSYMBOLOGYPALETTEMANAGER_H
