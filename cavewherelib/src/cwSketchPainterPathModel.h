/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCHPAINTERPATHMODEL_H
#define CWSKETCHPAINTERPATHMODEL_H

//Qt includes
#include <QColor>
#include <QLineF>
#include <QObject>
#include <QObjectBindableProperty>
#include <QPainterPath>
#include <QPointer>
#include <QQmlEngine>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwSketchPathSource.h"
#include "cwPaletteSnapshot.h"
#include "cwPenPoint.h"

class cwSketch;

class CAVEWHERE_LIB_EXPORT cwSketchPainterPathModel : public QObject, public cwSketchPathSource
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SketchPainterPathModel)

    Q_PROPERTY(int activeStrokeIndex READ activeStrokeIndex WRITE setActiveStrokeIndex NOTIFY activeStrokeIndexChanged BINDABLE bindableActiveStrokeIndex)
    Q_PROPERTY(QColor wallStrokeColor READ wallStrokeColor WRITE setWallStrokeColor NOTIFY wallStrokeColorChanged)
    Q_PROPERTY(QColor nonWallStrokeColor READ nonWallStrokeColor WRITE setNonWallStrokeColor NOTIFY nonWallStrokeColorChanged)

public:
    explicit cwSketchPainterPathModel(QObject *parent = nullptr);

    // Read-only stroke source. const because the painter only reads strokes()
    // and connects to change signals (which accept a const sender).
    const cwSketch *sketch() const;
    void setSketch(const cwSketch *sketch);

    int activeStrokeIndex() const { return m_activeStrokeIndex.value(); }
    void setActiveStrokeIndex(int index) { m_activeStrokeIndex = index; }
    QBindable<int> bindableActiveStrokeIndex() { return &m_activeStrokeIndex; }

    // Theme colors for the two stroke classes. A stroke's brush picks between
    // them via its scrapOutline flag (wall-class brushes use wallStrokeColor).
    // Per-brush ink colours replace this in commit 5.
    QColor wallStrokeColor() const { return m_wallStrokeColor; }
    void setWallStrokeColor(const QColor &color);

    QColor nonWallStrokeColor() const { return m_nonWallStrokeColor; }
    void setNonWallStrokeColor(const QColor &color);

    // cwSketchPathSource. Element 0 is the in-progress active stroke (empty
    // when none is active); the remainder are finished strokes batched by
    // colour.
    QList<Path> paths() const override;

signals:
    void activeStrokeIndexChanged();
    void wallStrokeColorChanged();
    void nonWallStrokeColorChanged();

    // Emitted whenever paths() would return a different result.
    void pathsChanged();

private:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(
        cwSketchPainterPathModel, int, m_activeStrokeIndex, -1,
        &cwSketchPainterPathModel::activeStrokeIndexChanged);

    QPointer<const cwSketch> m_sketch;
    Path m_activePath;
    QList<Path> m_finishedPaths;

    int m_previousActiveStroke = -1;

    double m_maxHalfWidth = 3.0;
    double m_minHalfWidth = 0.25;
    double m_widthScale   = 1.5;
    int    m_endPointTessellation = 5;

    int strokeCount() const;

    // Resolves stroke brushNames to brushes. Seeded with the built-in palette;
    // commit 5 threads in the sketch's active-palette snapshot.
    cwPaletteSnapshot m_snapshot;

    QVector<cwPenPoint> strokePoints(int row) const;
    double              strokeWidth(int row) const;
    QColor              strokeColor(int row) const;
    void                scheduleColorRebuild();

    QColor m_wallStrokeColor    = Qt::black;
    QColor m_nonWallStrokeColor = Qt::black;
    bool   m_colorRebuildPending = false;

    void onStrokeInserted(int row);
    void onStrokeRemoved(int row);
    void onStrokeChanged(int row);
    void onStrokesReset();
    void onActiveStrokeIndexChanged();

    void updateActivePath();
    void rebuildAllFinished();
    void addOrUpdateFinishPath(int sourceRow);
    bool mergeFinishPath(int sourceRow);

    void buildStrokeGeometry(QPainterPath &out, int sourceRow) const;
};

#endif // CWSKETCHPAINTERPATHMODEL_H
