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
    // fresh id, picks a collision-free subdirectory under paletteDirectory(),
    // appends the new palette in memory, and returns it. The fork is persisted
    // asynchronously — palettesChanged drives cwSaveLoad to write the whole
    // palette (and create its directory) as a first-class save job; nothing is
    // written to disk synchronously. Returns nullptr on failure (the reason is
    // pushed to errorModel()), including when no project is open. Existing
    // palette pointers (including `source`) stay valid — no reload happens.
    Q_INVOKABLE cwSymbologyPalette *duplicatePalette(cwSymbologyPalette *source,
                                                     const QString &newName);

    // Upsert a glyph on a writable palette (object-first): mutates the live
    // palette and emits glyphChanged. Persistence is asynchronous — cwSaveLoad's
    // connectPalette writes the glyph file as a first-class save job. No-op
    // returning false if the palette is null or read-only. Used by the glyph
    // editor's Save.
    Q_INVOKABLE bool saveGlyph(cwSymbologyPalette *palette, const cwSymbologyGlyph &glyph);

    // Drop a glyph from a writable palette (object-first, a no-op if absent).
    // cwSaveLoad removes the glyph file asynchronously. A rename is
    // saveGlyph(newGlyph) followed by removeGlyph(oldName).
    Q_INVOKABLE bool removeGlyph(cwSymbologyPalette *palette, const QString &glyphName);

    // Copy an existing glyph into a new one with a derived unique name
    // ("<name>-copy", then "<name>-copy-2"…) and a "<displayName> copy" label.
    // Object-first: deep-copies the strokes and setGlyph()s the copy (→
    // glyphChanged → it appears in the list); cwSaveLoad persists it
    // asynchronously. Returns the new glyph's name so the caller can select it,
    // or an empty QString on failure (null/read-only palette, or `name` is not a
    // member) with the reason pushed to errorModel().
    Q_INVOKABLE QString duplicateGlyph(cwSymbologyPalette *palette, const QString &name);

    // Remove a writable palette from the project: drops it from the list,
    // deletes the object (so any QML binding to it must release), and emits
    // palettesChanged. The palette's whole on-disk directory is torn down
    // asynchronously by cwSaveLoad (a recursive directory Remove on the save
    // queue). Returns false (with a reason on errorModel()) for a null,
    // read-only (e.g. the shipped default), or unknown palette. The passed
    // pointer is dangling on success — do not use it afterward.
    Q_INVOKABLE bool removePalette(cwSymbologyPalette *palette);

    // The project-root subdirectory name that holds a project's forked palettes
    // ("palettes"). cwSaveLoad joins this onto the project root and points the
    // manager at it on every project open / new / save-as — the shipped default
    // palette is embedded in qrc and loaded regardless of this directory.
    static QString folderName();

signals:
    void palettesChanged();

    // Emitted at the very start of reload(), before the directory is rescanned
    // and surviving palettes are setData()'d. cwSaveLoad uses it to drop its
    // per-palette auto-save wiring around the load so a just-loaded palette is
    // never written back to disk (which would defeat conflict-free sync), and to
    // mark the reloaded palettes as already-persisted.
    void aboutToReload();

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
