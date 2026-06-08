/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPALETTESNAPSHOT_H
#define CWPALETTESNAPSHOT_H

//Qt includes
#include <QHash>
#include <QString>

//Std includes
#include <optional>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwLineBrush.h"
#include "cwSymbologyGlyph.h"

// Value-type, implicitly-shared lookup surface that every render-path consumer
// (decoration generation, scene rules, offset-curve batching) takes by value.
// Constructing or copying one is a refcount bump on its internal
// QHash, not a deep copy. cwSketchPainter::paint grabs a snapshot from the
// active palette at the start of each repaint and threads it downstream, so
// UI-side palette mutations during a paint pass leave the snapshot intact.
//
// findBrush returns a cwLineBrush *by value*: copying it is cheap because its
// members are all copy-on-write (the QStrings and the QVector of decoration
// layers), so the copy is a handful of atomic refcount bumps, no deep copy. The
// returned brush stays valid no matter what happens to the snapshot or the live
// palette afterward — no dangling pointer the way a `cwLineBrush const*` into
// the hash would invite.
class CAVEWHERE_LIB_EXPORT cwPaletteSnapshot {
public:
    cwPaletteSnapshot() = default;
    cwPaletteSnapshot(QHash<QString, cwLineBrush> brushes,
                      QHash<QString, cwSymbologyGlyph> glyphs);

    // const QString& (not QStringView): the backing QHash<QString, ...> has no
    // heterogeneous lookup, so a view would force a per-call QString allocation.
    std::optional<cwLineBrush> findBrush(const QString &name) const; // nullopt if missing

    // Glyph lookup used by decoration layout and the tessellation cache; same
    // by-value, lifetime-safe contract as findBrush.
    std::optional<cwSymbologyGlyph> findGlyph(const QString &name) const; // nullopt if missing

    // True when `name` resolves to a brush whose strokes contribute to scrap
    // outline detection (the "wall-class" test shared by the detector, scrap
    // manager and live painter). Reads the flag in place — no brush copy, unlike
    // findBrush(name) && findBrush(name)->scrapOutline.
    bool producesScrapOutline(const QString &name) const;

    bool isEmpty() const { return m_brushes.isEmpty() && m_glyphs.isEmpty(); }
    int  brushCount() const { return m_brushes.size(); }
    int  glyphCount() const { return m_glyphs.size(); }

private:
    QHash<QString, cwLineBrush> m_brushes;
    QHash<QString, cwSymbologyGlyph> m_glyphs;
};

#endif // CWPALETTESNAPSHOT_H
