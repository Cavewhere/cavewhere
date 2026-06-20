/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSYMBOLOGYNAME_H
#define CWSYMBOLOGYNAME_H

//Qt includes
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringView>

//Our includes
#include "CaveWhereLibExport.h"

// The machine-name rule shared by line brushes and glyphs. A symbology name is
// both the cross-palette substitution key (cwPenStroke::brushName, a decoration
// layer's glyphName) and a path component on disk (brushes/<name>.cwbrush,
// glyphs/<name>.cwglyph), so a single kebab-case rule serves identity and
// path-safety at once.
namespace cwSymbology {

// True when `name` is a non-empty kebab-case identifier: lowercase ASCII
// letters, digits and interior hyphens, with no leading or trailing hyphen.
// Rejects path separators, "..", dots and uppercase, so a name read from an
// untrusted palette cannot escape its directory.
CAVEWHERE_LIB_EXPORT bool isValidName(QStringView name);

}

// QML-accessible facade for the shared name rule, registered as the singleton
// `SymbologyName`. Name-authoring editors (the glyph library, and the brush
// editor to come) gate input through `SymbologyName.isValidName(...)` so they
// validate on the exact rule cwSymbologyPaletteIO and cwSaveLoad enforce — one
// validation surface, rather than each editor hanging a wrapper off whatever
// canvas it owns or duplicating a kebab-case regex in QML.
class CAVEWHERE_LIB_EXPORT cwSymbologyName : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SymbologyName)
    QML_SINGLETON

public:
    explicit cwSymbologyName(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE bool isValidName(const QString &name) const { return cwSymbology::isValidName(name); }
};

#endif // CWSYMBOLOGYNAME_H
