/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwGlyphListModel.h"
#include "cwSymbologyPalette.h"

//Std includes
#include <algorithm>

cwGlyphListModel::cwGlyphListModel(QObject *parent) :
    QAbstractListModel(parent)
{
}

void cwGlyphListModel::setPalette(cwSymbologyPalette *palette)
{
    if (m_palette == palette) {
        return;
    }

    disconnect(m_glyphConnection);
    disconnect(m_dataConnection);
    m_palette = palette;
    if (m_palette != nullptr) {
        // Object-first edits (setGlyph/removeGlyph) emit only the name-scoped
        // glyphChanged — that is the add/remove/edit path the library uses.
        m_glyphConnection = connect(m_palette, &cwSymbologyPalette::glyphChanged,
                                    this, &cwGlyphListModel::refresh);
        // A whole-palette replace (setData on a manager reload) arrives here.
        m_dataConnection = connect(m_palette, &cwSymbologyPalette::dataChanged,
                                   this, &cwGlyphListModel::refresh);
    }

    refresh();
    emit paletteChanged();
}

void cwGlyphListModel::refresh()
{
    beginResetModel();
    m_glyphs = m_palette ? sortedForList(m_palette->glyphs()) : QVector<cwSymbologyGlyph>();
    endResetModel();
}

QVector<cwSymbologyGlyph> cwGlyphListModel::sortedForList(QVector<cwSymbologyGlyph> glyphs)
{
    std::stable_sort(glyphs.begin(), glyphs.end(),
                     [](const cwSymbologyGlyph &a, const cwSymbologyGlyph &b) {
        return QString::localeAwareCompare(a.displayName, b.displayName) < 0;
    });
    return glyphs;
}

int cwGlyphListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_glyphs.size());
}

QVariant cwGlyphListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_glyphs.size()) {
        return QVariant();
    }

    const cwSymbologyGlyph &glyph = m_glyphs.at(index.row());
    switch (role) {
    case NameRole:
        return glyph.name;
    case DisplayNameRole:
        return glyph.displayName;
    case StrokeCountRole:
        return static_cast<int>(glyph.strokes.size());
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> cwGlyphListModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {DisplayNameRole, "displayName"},
        {StrokeCountRole, "strokeCount"}
    };
}
