/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPaletteSnapshot.h"

cwPaletteSnapshot::cwPaletteSnapshot(QHash<QString, cwLineBrush> brushes,
                                     QHash<QString, cwSymbologyGlyph> glyphs) :
    m_brushes(std::move(brushes)),
    m_glyphs(std::move(glyphs))
{
}

std::optional<cwLineBrush> cwPaletteSnapshot::findBrush(const QString &name) const
{
    const auto it = m_brushes.constFind(name);
    if (it == m_brushes.constEnd()) {
        return std::nullopt;
    }
    return *it;
}

std::optional<cwSymbologyGlyph> cwPaletteSnapshot::findGlyph(const QString &name) const
{
    const auto it = m_glyphs.constFind(name);
    if (it == m_glyphs.constEnd()) {
        return std::nullopt;
    }
    return *it;
}

bool cwPaletteSnapshot::producesScrapOutline(const QString &name) const
{
    const auto it = m_brushes.constFind(name);
    return it != m_brushes.constEnd() && it->scrapOutline;
}
