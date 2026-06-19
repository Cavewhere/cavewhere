/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSketchGlyphCanvas.h"
#include "cwSketch.h"
#include "cwScale.h"
#include "cwSymbologyPalette.h"
#include "cwGlyphTessellationCache.h"

namespace {

// Default paper-mm authoring extent — a 10 x 10 mm sheet centered on the origin.
constexpr double kDefaultPaperExtentMm = 10.0;

}

cwSketchGlyphCanvas::cwSketchGlyphCanvas(QQuickItem *parent) :
    cwSketchCanvas(parent),
    m_glyphSketch(new cwSketch(this)),
    m_paperExtentMm(kDefaultPaperExtentMm, kDefaultPaperExtentMm)
{
    // The internal sketch defaults to 1:250 and has no trip/region; the palette
    // it draws against is injected via setPalette(), not the resolver chain.
    setSketch(m_glyphSketch);
}

cwSketchGlyphCanvas::~cwSketchGlyphCanvas() = default;

void cwSketchGlyphCanvas::setPalette(cwSymbologyPalette *palette)
{
    if (m_palette == palette) {
        return;
    }

    if (m_paletteDataConnection) {
        QObject::disconnect(m_paletteDataConnection);
        m_paletteDataConnection = {};
    }

    m_palette = palette;

    if (m_palette != nullptr) {
        // Re-skin whenever the edited palette's content changes — a brush edit,
        // or a glyph saved through the manager (which reloads it in place).
        m_paletteDataConnection =
            connect(m_palette, &cwSymbologyPalette::dataChanged,
                    this, [this]() { refreshPathSnapshot(); });
    }

    refreshPathSnapshot();
    emit paletteChanged();
}

void cwSketchGlyphCanvas::setPaperExtentMm(const QSizeF &extent)
{
    if (m_paperExtentMm == extent) {
        return;
    }
    m_paperExtentMm = extent;
    emit paperExtentMmChanged();
}

cwPaletteSnapshot cwSketchGlyphCanvas::snapshotForPathModel() const
{
    if (m_palette != nullptr) {
        return m_palette->snapshot();
    }
    return cwSketchCanvas::snapshotForPathModel();
}

double cwSketchGlyphCanvas::paperMmToWorldM() const
{
    cwScale *scale = m_glyphSketch->mapScale();
    return cwGlyphTessellationCache::paperMmToWorldM(scale != nullptr ? scale->scale() : 0.0);
}

void cwSketchGlyphCanvas::loadGlyph(const cwSymbologyGlyph &glyph)
{
    const double factor = paperMmToWorldM();

    QVector<cwPenStroke> worldStrokes;
    worldStrokes.reserve(glyph.strokes.size());
    for (const cwPenStroke &paperStroke : glyph.strokes) {
        cwPenStroke worldStroke = paperStroke;
        for (cwPenPoint &point : worldStroke.points) {
            point.position *= factor;
        }
        worldStrokes.append(worldStroke);
    }

    m_glyphSketch->setStrokes(worldStrokes);
    // The loaded glyph is the editing baseline, not an undoable edit.
    m_glyphSketch->undoStack()->clear();
}

cwSymbologyGlyph cwSketchGlyphCanvas::toGlyph(const QString &name, const QString &displayName) const
{
    const double factor = paperMmToWorldM();
    const double inverse = factor != 0.0 ? 1.0 / factor : 0.0;

    cwSymbologyGlyph glyph;
    glyph.name = name;
    glyph.displayName = displayName;
    glyph.strokes.reserve(m_glyphSketch->strokes().size());
    for (const cwPenStroke &worldStroke : m_glyphSketch->strokes()) {
        cwPenStroke paperStroke = worldStroke;
        for (cwPenPoint &point : paperStroke.points) {
            point.position *= inverse;
        }
        glyph.strokes.append(paperStroke);
    }
    return glyph;
}

void cwSketchGlyphCanvas::clear()
{
    m_glyphSketch->clearStrokes();
}

void cwSketchGlyphCanvas::loadGlyphNamed(const QString &name)
{
    const std::optional<cwSymbologyGlyph> glyph =
        m_palette != nullptr ? m_palette->glyph(name) : std::nullopt;
    if (glyph) {
        loadGlyph(*glyph);
    } else {
        clear();
    }
}
