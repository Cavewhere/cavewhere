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
#include "cwSymbologyGlyph.h"

// moc needs the complete cwSymbologyPalette to register the paletteById return
// metatype; Q_MOC_INCLUDE keeps it out of this header.
Q_MOC_INCLUDE("cwSymbologyPalette.h")

class cwSymbologyPalette;
class cwErrorModel;

// Owns the set of installed palettes. The built-in default palette is shipped
// embedded as a qrc resource (:/palettes/cavewhere-default) and is always
// present. Forked (writable) palettes live inside the open project, under the
// project root's palettes/ subdirectory (folderName()); cwSaveLoad points the
// manager at that directory on every project open / new / save-as, the same way
// LAZ layers resolve "GIS Layers". The directory is scanned for subdirectories
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

    // The directory holding the open project's forked palettes (project
    // root + folderName()). Empty until a project is opened — with no writable
    // directory the manager still serves the shipped default. Setting it rescans.
    QString paletteDirectory() const { return m_paletteDirectory; }
    void setPaletteDirectory(const QString &directory);

    // The model of non-fatal load problems from the most recent reload(): failed
    // palettes, duplicate-id skips, forward-incompatible palettes, and rule-stack
    // warnings. Owned by the manager and refreshed on each reload(); bind to it
    // for UI display. fatalCount stays 0 — fatal problems fail the palette load
    // outright rather than landing here.
    cwErrorModel *errorModel() const { return m_errorModel; }

    // Rescan the palette directory. The default is always re-added first.
    void reload();

    // Default first, then installed palettes in scan order.
    QList<cwSymbologyPalette *> palettes() const { return m_palettes; }
    cwSymbologyPalette *defaultPalette() const { return m_default; }

    // Q_INVOKABLE so QML (the missing-palette banner) can ask whether a stored
    // region/settings id still resolves without scanning the palette list.
    Q_INVOKABLE cwSymbologyPalette *paletteById(const QUuid &id) const;

    // Fork an existing palette (the read-only seed or any installed one) into a
    // new writable user palette: deep-copies its brushes and glyphs, assigns a
    // fresh id, writes a new subdirectory under paletteDirectory(), reloads, and
    // returns the new palette. Returns nullptr on failure (the reason is pushed
    // to errorModel()). This is how a user gets something editable, since the
    // seed is read-only. The reload() reconciles by id, so existing palette
    // pointers (including `source`) stay valid across it.
    Q_INVOKABLE cwSymbologyPalette *duplicatePalette(cwSymbologyPalette *source,
                                                     const QString &newName);

    // Write a single glyph into a writable palette's glyphs/ subdir, then reload.
    // The reload reconciles by id, so the palette's pointer stays valid. No-op
    // returning false if the palette is null or read-only. Used by the glyph
    // editor's Save.
    Q_INVOKABLE bool saveGlyph(cwSymbologyPalette *palette, const cwSymbologyGlyph &glyph);

    // Delete a glyph file from a writable palette, then reload (the palette's
    // pointer survives). A rename is saveGlyph(newGlyph) followed by
    // removeGlyph(oldName).
    Q_INVOKABLE bool removeGlyph(cwSymbologyPalette *palette, const QString &glyphName);

    // The project-root subdirectory name that holds a project's forked palettes
    // ("palettes"). cwSaveLoad joins this onto the project root and points the
    // manager at it on every project open / new / save-as — the shipped default
    // palette is embedded in qrc and loaded regardless of this directory.
    static QString folderName();

signals:
    void palettesChanged();

private:
    // Record a non-fatal reload problem into the owned error model (de-duped,
    // qWarning'd). The QString overload is a Warning convenience; the cwError
    // overload preserves a typed error's severity and SymbologyErrorCode (the
    // editor keys off the code).
    void reportLoadProblem(const cwError &error);
    void reportLoadProblem(const QString &message);

    static cwSymbologyPaletteManager *Singleton;

    QString m_paletteDirectory;
    cwSymbologyPalette *m_default = nullptr;
    QList<cwSymbologyPalette *> m_palettes;
    cwErrorModel *m_errorModel = nullptr;
};

#endif // CWSYMBOLOGYPALETTEMANAGER_H
