/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSYMBOLOGYNAME_H
#define CWSYMBOLOGYNAME_H

//Qt includes
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

#endif // CWSYMBOLOGYNAME_H
