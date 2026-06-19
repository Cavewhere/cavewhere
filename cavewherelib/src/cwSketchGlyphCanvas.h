/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCHGLYPHCANVAS_H
#define CWSKETCHGLYPHCANVAS_H

//Qt includes
#include <QPointer>
#include <QQmlEngine>
#include <QSizeF>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwSketchCanvas.h"
#include "cwSymbologyGlyph.h"
// cwSketchCanvas exposes inline accessors over QPointer<cwScrapManager>/<cwTrip>
// members it only forward-declares; pull the complete types in here so every
// consumer of this header can instantiate them.
#include "cwScrapManager.h"
#include "cwTrip.h"

// moc needs the complete cwSymbologyPalette to register the palette property
// metatype; Q_MOC_INCLUDE keeps it out of this header.
Q_MOC_INCLUDE("cwSymbologyPalette.h")

class cwSketch;
class cwSymbologyPalette;

// A cwSketchCanvas specialized for authoring a single glyph in isolation. It
// owns its own cwSketch (no trip, no region) and draws against a palette
// injected directly via setPalette(), bypassing the region resolver chain — the
// palette being edited may be a fresh writable fork that is no region's default.
//
// A glyph stores its strokes in glyph-local paper-mm; the canvas works in
// world-m at the app-default 1:250 scale (so zoom/feel match a normal sketch),
// so loadGlyph()/toGlyph() convert through
// cwGlyphTessellationCache::paperMmToWorldM and its inverse — a lossless linear
// factor.
class CAVEWHERE_LIB_EXPORT cwSketchGlyphCanvas : public cwSketchCanvas
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SketchGlyphCanvas)

    // Authoring palette, injected directly. Setting it feeds the snapshot to the
    // inherited path source and re-skins on the palette's content changes.
    Q_PROPERTY(cwSymbologyPalette *palette READ palette WRITE setPalette NOTIFY paletteChanged)

    // Fixed paper-mm authoring extent (default 10 x 10 mm); drives the overlay.
    Q_PROPERTY(QSizeF paperExtentMm READ paperExtentMm WRITE setPaperExtentMm NOTIFY paperExtentMmChanged)

public:
    explicit cwSketchGlyphCanvas(QQuickItem *parent = nullptr);
    // Out-of-line so the inherited QPointer members (forward-declared in the
    // base header) are destroyed where their types are complete — keeps the
    // base's heavy includes out of every consumer TU.
    ~cwSketchGlyphCanvas() override;

    cwSymbologyPalette *palette() const { return m_palette; }
    void setPalette(cwSymbologyPalette *palette);

    QSizeF paperExtentMm() const { return m_paperExtentMm; }
    void setPaperExtentMm(const QSizeF &extent);

    // Load an existing glyph for editing (paper-mm -> world-m), replacing the
    // current strokes and resetting the undo baseline. toGlyph serializes the
    // current strokes back to a cwSymbologyGlyph (world-m -> paper-mm).
    Q_INVOKABLE void loadGlyph(const cwSymbologyGlyph &glyph);
    Q_INVOKABLE cwSymbologyGlyph toGlyph(const QString &name, const QString &displayName) const;
    Q_INVOKABLE void clear();

    // QML convenience: look `name` up in the injected palette and loadGlyph() it,
    // or clear() to a blank sheet if the name is empty or unknown (a new glyph).
    // cwSymbologyGlyph is not a QML gadget, so the editor can't pull a glyph out
    // of the palette itself.
    Q_INVOKABLE void loadGlyphNamed(const QString &name);

protected:
    cwPaletteSnapshot snapshotForPathModel() const override;

signals:
    void paletteChanged();
    void paperExtentMmChanged();

private:
    // Paper-mm <-> world-m factor at the canvas's current map scale.
    double paperMmToWorldM() const;

    cwSketch *m_glyphSketch = nullptr;
    QPointer<cwSymbologyPalette> m_palette;
    QMetaObject::Connection m_paletteDataConnection;
    QSizeF m_paperExtentMm;
};

#endif // CWSKETCHGLYPHCANVAS_H
