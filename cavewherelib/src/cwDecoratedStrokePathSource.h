/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWDECORATEDSTROKEPATHSOURCE_H
#define CWDECORATEDSTROKEPATHSOURCE_H

//Qt includes
#include <QColor>
#include <QObject>
#include <QPainterPath>
#include <QPointer>
#include <QQmlEngine>
#include <Qt>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwGlyphTessellationCache.h"
#include "cwPaletteSnapshot.h"
#include "cwSketchDecorationLayout.h"
#include "cwSketchPathSource.h"

class cwSketch;
struct cwGlyphSubPath;

// Live stroke source for the sketch canvas: each finished stroke is run through
// the decoration layout (cwSketchDecorationLayout) and its world-metre ink —
// traced lines and stamped glyphs — is flattened into the painter's draw list.
// The in-progress (active) stroke renders as a plain neutral centerline preview
// (Decision C) so a growing line doesn't re-place glyphs on every pen move.
//
// Finished ink is batched by resolved style (Decision E): every sub-path sharing
// a (color pair, width, cap/join, z, fill) tuple folds into one Path, so the
// painter holds a handful of items rather than one per glyph. Each batch keeps
// its source light/dark color pair, so a color-scheme toggle re-resolves the
// drawn color without re-running the layout (Decision D).
class CAVEWHERE_LIB_EXPORT cwDecoratedStrokePathSource : public QObject, public cwSketchPathSource
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DecoratedStrokePathSource)

    Q_PROPERTY(int activeStrokeIndex READ activeStrokeIndex WRITE setActiveStrokeIndex NOTIFY activeStrokeIndexChanged)
    Q_PROPERTY(QColor activeStrokeColor READ activeStrokeColor WRITE setActiveStrokeColor NOTIFY activeStrokeColorChanged)

public:
    explicit cwDecoratedStrokePathSource(QObject *parent = nullptr);

    // Read-only stroke source. const because the painter only reads strokes()
    // and connects to change signals (which accept a const sender).
    const cwSketch *sketch() const;
    void setSketch(const cwSketch *sketch);

    int activeStrokeIndex() const { return m_activeStrokeIndex; }
    void setActiveStrokeIndex(int index);

    // Neutral preview color for the in-progress stroke (Decision C). Finished
    // strokes take their per-brush ink from the palette snapshot instead.
    QColor activeStrokeColor() const { return m_activeStrokeColor; }
    void setActiveStrokeColor(const QColor &color);

    // Palette source of truth (from cwSketch::paletteSnapshot()); also feeds the
    // glyph tessellation cache. Replacing it rebuilds every finished batch.
    void setSnapshot(const cwPaletteSnapshot &snapshot);

    // Paper:world ratio (e.g. 1/250) from cwSketch::mapScale()->scale(). Symbology
    // is authored in paper-mm and baked to world-m, so a scale change re-runs the
    // layout for all finished strokes.
    void setMapScaleRatio(double ratio);
    double mapScaleRatio() const { return m_mapScaleRatio; }

    // cwSketchPathSource. Element 0 is the active-stroke preview (empty when none
    // is active); the rest are finished-stroke batches. The painter z-sorts.
    QList<Path> paths() const override;

signals:
    void activeStrokeIndexChanged();
    void activeStrokeColorChanged();

    // Emitted whenever paths() would return a different result.
    void pathsChanged();

private:
    // A finished-stroke draw batch: the public Path the painter consumes, plus
    // the source color pair so a theme toggle re-resolves without re-layout.
    // Fill and pen passes never share a batch — a fill carries strokeWidth <= 0,
    // a pen a positive world width, so the style key keeps them apart.
    struct Batch {
        Path   path;
        QColor srcLight;
        QColor srcDark;
    };

    QPointer<const cwSketch> m_sketch;

    cwGlyphTessellationCache m_cache;
    cwSketchDecorationLayout m_layout;          // {&m_cache}; const transform

    cwPaletteSnapshot m_snapshot;
    double m_mapScaleRatio = 1.0 / 250.0;

    int m_activeStrokeIndex   = -1;
    int m_previousActiveStroke = -1;

    bool m_darkMode = false;

    QList<Batch> m_finished;
    Path         m_activePath;
    QColor       m_activeStrokeColor = Qt::black;

    int strokeCount() const;
    QVector<QPointF> strokeCenterline(int row) const;

    void onStrokeInserted(int row);
    void onStrokeRemoved(int row);
    void onStrokeChanged(int row);
    void onStrokesReset();
    void onActiveStrokeIndexChanged();
    void onColorSchemeChanged(Qt::ColorScheme scheme);

    // The single rebuild seam (Decision A): synchronous today, swappable for an
    // async dispatch later without touching the path-source contract.
    void scheduleRebuild();
    void rebuildAllFinished();
    void foldFinishedStroke(int row);
    void reResolveColors();
    void buildActivePath();
    void buildPreviewGeometry(QPainterPath &out, int row) const;

    void foldSubPath(const cwGlyphSubPath &sub, Qt::PenCapStyle cap,
                     Qt::PenJoinStyle join, double z);
    Batch &batchFor(const QColor &srcLight, const QColor &srcDark,
                    double width, bool widthInWorldMetres,
                    Qt::PenCapStyle cap, Qt::PenJoinStyle join, double z);
    QColor resolveColor(const QColor &light, const QColor &dark) const;
};

#endif // CWDECORATEDSTROKEPATHSOURCE_H
