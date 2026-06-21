/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPaletteBackedSnapshotSource.h"
#include "cwSymbologyPalette.h"

cwPaletteBackedSnapshotSource::cwPaletteBackedSnapshotSource(QObject *parent) :
    cwPaletteSnapshotSource(parent)
{
}

void cwPaletteBackedSnapshotSource::setPalette(cwSymbologyPalette *palette)
{
    if (m_palette == palette) {
        return;
    }

    if (m_dataConnection) {
        QObject::disconnect(m_dataConnection);
        m_dataConnection = {};
    }

    m_palette = palette;

    if (m_palette != nullptr) {
        // Re-skin whenever the edited palette's content changes — a brush edit,
        // or a glyph saved through the manager (which reloads it in place).
        m_dataConnection =
            connect(m_palette, &cwSymbologyPalette::dataChanged,
                    this, &cwPaletteBackedSnapshotSource::snapshotChanged);
    }

    emit snapshotChanged();
}

cwPaletteSnapshot cwPaletteBackedSnapshotSource::snapshot() const
{
    return m_palette != nullptr ? m_palette->snapshot() : cwPaletteSnapshot();
}
