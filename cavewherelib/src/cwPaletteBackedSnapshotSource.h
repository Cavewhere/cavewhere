/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPALETTEBACKEDSNAPSHOTSOURCE_H
#define CWPALETTEBACKEDSNAPSHOTSOURCE_H

//Qt includes
#include <QPointer>

//Our includes
#include "cwPaletteSnapshotSource.h"

class cwSymbologyPalette;

// A snapshot source backed directly by a cwSymbologyPalette: snapshot() returns
// the palette's own snapshot (bypassing the region resolver), and it re-emits
// snapshotChanged() on the palette's dataChanged(). Used by cwSketchGlyphCanvas,
// which authors against a palette that may be no region's default.
class cwPaletteBackedSnapshotSource : public cwPaletteSnapshotSource
{
    Q_OBJECT

public:
    explicit cwPaletteBackedSnapshotSource(QObject *parent = nullptr);

    cwSymbologyPalette *palette() const { return m_palette; }
    void setPalette(cwSymbologyPalette *palette);

    cwPaletteSnapshot snapshot() const override;

private:
    QPointer<cwSymbologyPalette> m_palette;
    QMetaObject::Connection m_dataConnection;
};

#endif // CWPALETTEBACKEDSNAPSHOTSOURCE_H
