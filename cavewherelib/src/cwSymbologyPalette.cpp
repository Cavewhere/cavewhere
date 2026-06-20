/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSymbologyPalette.h"
#include "cwSymbologyName.h"

//Qt includes
#include <QSet>

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
    bool removeByName(QVector<T> &items, const QString &name)
    {
        for (int i = 0; i < items.size(); ++i) {
            if (items.at(i).name == name) {
                items.removeAt(i);
                return true;
            }
        }
        return false;
    }

    // Build the "<name>-copy" (then "-copy-2"…) duplicate of the named member, or
    // nullopt if `name` isn't a member. Both value structs carry .name/.displayName,
    // so one template serves glyphs and brushes alike.
    template <typename T>
    std::optional<T> makeDuplicate(const QVector<T> &items, const QString &name)
    {
        const T *source = nullptr;
        for (const T &item : items) {
            if (item.name == name) { source = &item; break; }
        }
        if (source == nullptr) { return std::nullopt; }

        QSet<QString> taken;
        taken.reserve(items.size());
        for (const T &item : items) { taken.insert(item.name); }
        const QString copyName =
            cwSymbology::uniqueName(taken, cwSymbology::slugify(name + QStringLiteral("-copy")));

        T copy = *source;
        copy.name = copyName;
        copy.displayName = source->displayName.isEmpty()
                               ? QString()
                               : source->displayName + QStringLiteral(" copy");
        return copy;
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
    if (!m_writable) { return; }
    if (upsertByName(m_data.glyphs, glyph)) {
        emit glyphChanged(glyph.name);
    }
}

void cwSymbologyPalette::removeGlyph(const QString &name)
{
    if (!m_writable) { return; }
    if (removeByName(m_data.glyphs, name)) {
        emit glyphChanged(name);
    }
}

void cwSymbologyPalette::setBrush(const cwLineBrush &brush)
{
    if (!m_writable) { return; }
    if (upsertByName(m_data.lineBrushes, brush)) {
        emit brushChanged(brush.name);
    }
}

void cwSymbologyPalette::removeBrush(const QString &name)
{
    if (!m_writable) { return; }
    if (removeByName(m_data.lineBrushes, name)) {
        emit brushChanged(name);
    }
}

// The leading writable check is an early-out that skips the copy work on a
// read-only palette; the final setGlyph/setBrush enforces the guard for real.
QString cwSymbologyPalette::duplicateGlyph(const QString &name)
{
    if (!m_writable) { return QString(); }
    const std::optional<cwSymbologyGlyph> copy = makeDuplicate(m_data.glyphs, name);
    if (!copy) { return QString(); }
    setGlyph(*copy);
    return copy->name;
}

QString cwSymbologyPalette::duplicateBrush(const QString &name)
{
    if (!m_writable) { return QString(); }
    const std::optional<cwLineBrush> copy = makeDuplicate(m_data.lineBrushes, name);
    if (!copy) { return QString(); }
    setBrush(*copy);
    return copy->name;
}
