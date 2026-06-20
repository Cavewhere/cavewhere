/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGLYPHLISTMODEL_H
#define CWGLYPHLISTMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QQmlEngine>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwSymbologyGlyph.h"

// moc needs the complete cwSymbologyPalette to register the palette property
// metatype; Q_MOC_INCLUDE keeps it out of this header.
Q_MOC_INCLUDE("cwSymbologyPalette.h")

class cwSymbologyPalette;

// Flat list of the glyphs in `palette`, sorted alphabetically by displayName
// (locale-aware) for the glyph-library list. cwSymbologyGlyph is a plain struct
// (not a Q_GADGET), so QML cannot enumerate palette.glyphs() directly — this
// model is what exposes one row per glyph.
//
// Bind `palette` to the page's writable palette; glyphs are copied by value on
// set, so the model never dereferences the palette pointer again (safe across a
// manager reload that deletes the palette object).
//
// Unlike cwLineBrushListModel, this refreshes on glyphChanged(QString) — the
// glyph library mutates the palette object-first (setGlyph/removeGlyph emit only
// the name-scoped signal, never dataChanged()), so a dataChanged-only wiring
// would miss every add/remove/edit. dataChanged() (reload) is wired too.
class CAVEWHERE_LIB_EXPORT cwGlyphListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(GlyphListModel)

    Q_PROPERTY(cwSymbologyPalette* palette READ palette WRITE setPalette NOTIFY paletteChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1, // kebab-case machine id (the in-palette key)
        DisplayNameRole,             // human label
        StrokeCountRole              // cheap, for an empty-glyph hint
    };
    Q_ENUM(Role)

    explicit cwGlyphListModel(QObject *parent = nullptr);

    cwSymbologyPalette *palette() const { return m_palette; }
    void setPalette(cwSymbologyPalette *palette);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // The list ordering, exposed as a free function so it can be unit-tested
    // directly on a glyph vector without standing up a model + palette.
    static QVector<cwSymbologyGlyph> sortedForList(QVector<cwSymbologyGlyph> glyphs);

signals:
    void paletteChanged();

private:
    void refresh();

    cwSymbologyPalette *m_palette = nullptr;
    QMetaObject::Connection m_glyphConnection;
    QMetaObject::Connection m_dataConnection;
    QVector<cwSymbologyGlyph> m_glyphs; // sorted per sortedForList()
};

#endif // CWGLYPHLISTMODEL_H
