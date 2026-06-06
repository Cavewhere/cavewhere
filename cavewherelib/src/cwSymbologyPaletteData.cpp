/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSymbologyPaletteData.h"

//Qt includes
#include <QHash>
#include <QSet>

std::optional<cwLineBrush> cwSymbologyPaletteData::brush(QStringView name) const
{
    for (const auto &b : lineBrushes) {
        if (b.name == name) {
            return b;
        }
    }
    return std::nullopt;
}

cwPaletteSnapshot cwSymbologyPaletteData::snapshot() const
{
    QHash<QString, cwLineBrush> byName;
    byName.reserve(lineBrushes.size());
    for (const auto &b : lineBrushes) {
        byName.insert(b.name, b);
    }
    return cwPaletteSnapshot(std::move(byName));
}

QString cwSymbologyPaletteData::findDuplicateBrushName(const QVector<cwLineBrush> &brushes)
{
    QSet<QString> seen;
    seen.reserve(brushes.size());
    for (const auto &b : brushes) {
        if (seen.contains(b.name)) {
            return b.name;
        }
        seen.insert(b.name);
    }
    return QString();
}
