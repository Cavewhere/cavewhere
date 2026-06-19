/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSymbologyPalette.h"

namespace {
    // Replace the same-named element in place, append if new, or report no change
    // when the content already matches. Names are unique within a palette, so the
    // first match is the only one.
    template <typename T>
    bool upsertByName(QVector<T> &items, const T &item)
    {
        for (int i = 0; i < items.size(); ++i) {
            if (items.at(i).name == item.name) {
                if (items.at(i) == item) { return false; }
                items.replace(i, item);
                return true;
            }
        }
        items.append(item);
        return true;
    }

    template <typename T>
    bool removeByName(QVector<T> &items, QStringView name)
    {
        for (int i = 0; i < items.size(); ++i) {
            if (items.at(i).name == name) {
                items.removeAt(i);
                return true;
            }
        }
        return false;
    }
}

cwSymbologyPalette::cwSymbologyPalette(QObject *parent) :
    QObject(parent)
{
}

void cwSymbologyPalette::setData(const cwSymbologyPaletteData &data)
{
    if (m_data == data) { return; }
    m_data = data;
    emit dataChanged();
}

void cwSymbologyPalette::setWritable(bool writable)
{
    if (m_writable == writable) { return; }
    m_writable = writable;
    emit writableChanged();
}

void cwSymbologyPalette::setGlyph(const cwSymbologyGlyph &glyph)
{
    if (upsertByName(m_data.glyphs, glyph)) {
        emit glyphChanged(glyph.name);
    }
}

void cwSymbologyPalette::removeGlyph(QStringView name)
{
    if (removeByName(m_data.glyphs, name)) {
        emit glyphChanged(name.toString());
    }
}

void cwSymbologyPalette::setBrush(const cwLineBrush &brush)
{
    if (upsertByName(m_data.lineBrushes, brush)) {
        emit brushChanged(brush.name);
    }
}

void cwSymbologyPalette::removeBrush(QStringView name)
{
    if (removeByName(m_data.lineBrushes, name)) {
        emit brushChanged(name.toString());
    }
}
