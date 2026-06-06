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
    explicit cwPaletteSnapshot(QHash<QString, cwLineBrush> brushes);

    // const QString& (not QStringView): the backing QHash<QString, ...> has no
    // heterogeneous lookup, so a view would force a per-call QString allocation.
    std::optional<cwLineBrush> findBrush(const QString &name) const; // nullopt if missing

    bool isEmpty() const { return m_brushes.isEmpty(); }
    int  brushCount() const { return m_brushes.size(); }

private:
    QHash<QString, cwLineBrush> m_brushes;
};

#endif // CWPALETTESNAPSHOT_H
