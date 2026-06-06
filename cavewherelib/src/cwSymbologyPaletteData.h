/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSYMBOLOGYPALETTEDATA_H
#define CWSYMBOLOGYPALETTEDATA_H

//Qt includes
#include <QMetaType>
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

// Serializable value payload of a palette (the cwTripData-style data half of
// the cwSymbologyPalette QObject). cwSymbologyPaletteIO reads and writes this
// value directly, returning Monad::Result<cwSymbologyPaletteData>, which keeps
// loading free of output-pointer plumbing and lets a palette be parsed on a
// worker thread and moved to the UI thread for async loading.
//
// Brush names are unique within the palette and are the cross-palette
// substitution key. The palette's identity is its UUID, not its directory name,
// so a clone retains identity regardless of path.
struct CAVEWHERE_LIB_EXPORT cwSymbologyPaletteData {
    QUuid   id;
    QString name;
    QString description;
    QString author;
    QString version;
    QVector<cwLineBrush> lineBrushes;

    std::optional<cwLineBrush> brush(QStringView name) const;

    // Cheap implicitly-shared lookup surface for the render path.
    cwPaletteSnapshot snapshot() const;

    // Returns the first brush name that appears more than once, or an empty
    // string if every name is unique. The uniqueness invariant the loader
    // enforces — names are the substitution key, so collisions are ambiguous.
    static QString findDuplicateBrushName(const QVector<cwLineBrush> &brushes);

    bool operator==(const cwSymbologyPaletteData &o) const = default;
};

Q_DECLARE_METATYPE(cwSymbologyPaletteData)

#endif // CWSYMBOLOGYPALETTEDATA_H
