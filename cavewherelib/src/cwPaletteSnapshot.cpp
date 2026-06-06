/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPaletteSnapshot.h"

cwPaletteSnapshot::cwPaletteSnapshot(QHash<QString, cwLineBrush> brushes) :
    m_brushes(std::move(brushes))
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

bool cwPaletteSnapshot::producesScrapOutline(const QString &name) const
{
    const auto it = m_brushes.constFind(name);
    return it != m_brushes.constEnd() && it->scrapOutline;
}
