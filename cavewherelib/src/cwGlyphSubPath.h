/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGLYPHSUBPATH_H
#define CWGLYPHSUBPATH_H

//Qt includes
#include <QColor>
#include <QPainterPath>

// One typed piece of glyph ink in world metres, plus the look its source brush
// imposes. A glyph stroke resolves its brush's terminal: a Trace brush with no
// fill yields a Stroke (pen only), a Trace brush carrying a fill yields a Polygon
// (pen + a fillable interior). cwGlyphTessellationCache emits these glyph-local (origin at
// the glyph anchor); cwSketchDecorationLayout reuses the same struct for a placed
// stamp sub-path, where `path` has been transformed onto the stroke.
//
// Both light and dark colours are carried because cave maps invert in dark mode,
// so per-symbol colours must store both variants; the consumer (commit 5 painter,
// the headless reference image) picks the one matching the active mode. A Stroke
// leaves the fill colours default-constructed (invalid).
struct cwGlyphSubPath {
    enum Kind { Stroke, Polygon };

    Kind         kind = Stroke;
    QPainterPath path;            // world metres
    QColor       penColorLight;
    QColor       penColorDark;
    double       penWidthMm = 0.0;
    QColor       fillColorLight;  // Polygon only
    QColor       fillColorDark;   // Polygon only
};

#endif // CWGLYPHSUBPATH_H
