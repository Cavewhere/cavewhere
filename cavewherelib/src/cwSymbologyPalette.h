/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSYMBOLOGYPALETTE_H
#define CWSYMBOLOGYPALETTE_H

//Qt includes
#include <QObject>
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
#include "cwSymbologyGlyph.h"
#include "cwSymbologyPaletteData.h"

// QObject wrapper around a cwSymbologyPaletteData (the cwTrip/cwTripData
// pattern): the value half round-trips through cwSymbologyPaletteIO, while this
// QObject is what the manager owns and QML observes. Mutation is wholesale via
// setData() — palettes are loaded and re-skinned as a unit, not field-by-field.
class CAVEWHERE_LIB_EXPORT cwSymbologyPalette : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QUuid id READ id NOTIFY dataChanged)
    Q_PROPERTY(QString name READ name NOTIFY dataChanged)
    Q_PROPERTY(QString description READ description NOTIFY dataChanged)
    Q_PROPERTY(QString author READ author NOTIFY dataChanged)
    Q_PROPERTY(QString version READ version NOTIFY dataChanged)
    Q_PROPERTY(bool writable READ isWritable WRITE setWritable NOTIFY writableChanged)

public:
    explicit cwSymbologyPalette(QObject *parent = nullptr);

    cwSymbologyPaletteData data() const { return m_data; }
    void setData(const cwSymbologyPaletteData &data);

    QUuid id() const { return m_data.id; }
    QString name() const { return m_data.name; }
    QString description() const { return m_data.description; }
    QString author() const { return m_data.author; }
    QString version() const { return m_data.version; }

    // Runtime-only flag (not persisted) — false for the built-in seed and any
    // read-only installed palette; true for palettes the user can edit.
    bool isWritable() const { return m_writable; }
    void setWritable(bool writable);

    QVector<cwLineBrush> lineBrushes() const { return m_data.lineBrushes; }
    QVector<cwSymbologyGlyph> glyphs() const { return m_data.glyphs; }

    std::optional<cwLineBrush> brush(QStringView name) const { return m_data.brush(name); }
    std::optional<cwSymbologyGlyph> glyph(QStringView name) const { return m_data.glyph(name); }

    // Cheap implicitly-shared lookup surface for the render path.
    cwPaletteSnapshot snapshot() const { return m_data.snapshot(); }

signals:
    void dataChanged();
    void writableChanged();

private:
    cwSymbologyPaletteData m_data;
    bool m_writable = false;
};

#endif // CWSYMBOLOGYPALETTE_H
