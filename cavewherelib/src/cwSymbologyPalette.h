/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSYMBOLOGYPALETTE_H
#define CWSYMBOLOGYPALETTE_H

//Qt includes
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringView>
#include <QUuid>
#include <QVector>

//Std includes
#include <optional>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwLineBrush.h"
#include "cwPaletteSnapshot.h"
#include "cwSymbologyGlyph.h"
#include "cwSymbologyPaletteData.h"

// QObject wrapper around a cwSymbologyPaletteData (the cwTrip/cwTripData
// pattern): the value half round-trips through cwSymbologyPaletteIO, while this
// QObject is what the manager owns and QML observes. setData() replaces the
// whole value (load / reconciling reload); setGlyph/setBrush/removeGlyph/
// removeBrush mutate a single member in place and emit a name-scoped change
// signal — the object-first edit path the auto-save glyph library builds on.
class CAVEWHERE_LIB_EXPORT cwSymbologyPalette : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SymbologyPalette)
    QML_UNCREATABLE("Palettes are owned by cwSymbologyPaletteManager")

    Q_PROPERTY(QUuid id READ id NOTIFY dataChanged)
    Q_PROPERTY(QString name READ name NOTIFY dataChanged)
    Q_PROPERTY(QString description READ description NOTIFY dataChanged)
    Q_PROPERTY(QString author READ author NOTIFY dataChanged)
    Q_PROPERTY(QString version READ version NOTIFY dataChanged)
    Q_PROPERTY(bool writable READ isWritable WRITE setWritable NOTIFY writableChanged)

public:
    explicit cwSymbologyPalette(QObject *parent = nullptr);

    cwSymbologyPaletteData data() const { return m_data; }
    void setData(const cwSymbologyPaletteData &data);

    QUuid id() const { return m_data.id; }
    QString name() const { return m_data.name; }
    QString description() const { return m_data.description; }
    QString author() const { return m_data.author; }
    QString version() const { return m_data.version; }

    // Runtime-only flag (not persisted) — false for the built-in seed and any
    // read-only installed palette; true for palettes the user can edit.
    bool isWritable() const { return m_writable; }
    void setWritable(bool writable);

    // Runtime-only locator (not persisted) — the on-disk directory this palette
    // was loaded from, set by cwSymbologyPaletteManager during reload(). The
    // seed's is a qrc path (":/palettes/cavewhere-default"); writable palettes
    // get a filesystem path the editor writes glyph/brush files into.
    QString directory() const { return m_directory; }
    void setDirectory(const QString &directory) { m_directory = directory; }

    QVector<cwLineBrush> lineBrushes() const { return m_data.lineBrushes; }
    QVector<cwSymbologyGlyph> glyphs() const { return m_data.glyphs; }

    std::optional<cwLineBrush> brush(QStringView name) const { return m_data.brush(name); }
    std::optional<cwSymbologyGlyph> glyph(QStringView name) const { return m_data.glyph(name); }

    // QML convenience for selecting a member by name alone (e.g. after Duplicate):
    // the glyph/brush displayName, or an empty string if the name is not a member.
    // The value structs aren't QML gadgets, so QML can't read displayName off
    // glyph()/brush().
    Q_INVOKABLE QString glyphDisplayName(const QString &name) const
    {
        const std::optional<cwSymbologyGlyph> found = m_data.glyph(name);
        return found ? found->displayName : QString();
    }
    Q_INVOKABLE QString brushDisplayName(const QString &name) const
    {
        const std::optional<cwLineBrush> found = m_data.brush(name);
        return found ? found->displayName : QString();
    }

    // Single-member mutation, keyed by the unique name. setGlyph/setBrush upsert
    // (replace in place if the name exists, append otherwise) and are no-ops when
    // the content is unchanged; removeGlyph/removeBrush drop the named member if
    // present. Each emits glyphChanged/brushChanged with the affected name only
    // when something actually changed — a removal emits it too, so a consumer's
    // name-scoped invalidation covers a deleted member as well. All four are
    // writable-guarded: a no-op on a read-only palette (the UI gates this; a
    // direct caller hitting the guard is a programmer error and warns at most).
    Q_INVOKABLE void setGlyph(const cwSymbologyGlyph &glyph);
    Q_INVOKABLE void removeGlyph(const QString &name);
    Q_INVOKABLE void setBrush(const cwLineBrush &brush);
    Q_INVOKABLE void removeBrush(const QString &name);

    // Copy an existing member into a new one with a derived unique name
    // ("<name>-copy", then "<name>-copy-2"…) and a "<displayName> copy" label,
    // then upsert it (→ glyphChanged/brushChanged → it appears in lists; cwSaveLoad
    // persists it). Returns the new name so the caller can select it, or an empty
    // QString on failure (read-only palette, or `name` is not a member).
    Q_INVOKABLE QString duplicateGlyph(const QString &name);
    Q_INVOKABLE QString duplicateBrush(const QString &name);

    // Cheap implicitly-shared lookup surface for the render path.
    cwPaletteSnapshot snapshot() const { return m_data.snapshot(); }

signals:
    void dataChanged();
    void writableChanged();
    void glyphChanged(const QString &name);
    void brushChanged(const QString &name);

private:
    cwSymbologyPaletteData m_data;
    bool m_writable = false;
    QString m_directory;
};

#endif // CWSYMBOLOGYPALETTE_H
