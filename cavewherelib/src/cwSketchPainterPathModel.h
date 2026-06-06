/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCHPAINTERPATHMODEL_H
#define CWSKETCHPAINTERPATHMODEL_H

//Qt includes
#include <QAbstractItemModel>
#include <QColor>
#include <QLineF>
#include <QObjectBindableProperty>
#include <QPainterPath>
#include <QPointer>
#include <QQmlEngine>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwAbstractSketchPainterPathModel.h"
#include "cwPaletteSnapshot.h"
#include "cwPenPoint.h"

class CAVEWHERE_LIB_EXPORT cwSketchPainterPathModel : public cwAbstractSketchPainterPathModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SketchPainterPathModel)

    Q_PROPERTY(QAbstractItemModel *strokeModel READ strokeModel WRITE setStrokeModel NOTIFY strokeModelChanged)
    Q_PROPERTY(int activeStrokeIndex READ activeStrokeIndex WRITE setActiveStrokeIndex NOTIFY activeStrokeIndexChanged BINDABLE bindableActiveStrokeIndex)
    Q_PROPERTY(QColor wallStrokeColor READ wallStrokeColor WRITE setWallStrokeColor NOTIFY wallStrokeColorChanged)
    Q_PROPERTY(QColor nonWallStrokeColor READ nonWallStrokeColor WRITE setNonWallStrokeColor NOTIFY nonWallStrokeColorChanged)

public:
    explicit cwSketchPainterPathModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QAbstractItemModel *strokeModel() const;
    void setStrokeModel(QAbstractItemModel *model);

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

signals:
    void strokeModelChanged();
    void activeStrokeIndexChanged();
    void wallStrokeColorChanged();
    void nonWallStrokeColorChanged();

protected:
    Path path(const QModelIndex &index) const override;

private:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(
        cwSketchPainterPathModel, int, m_activeStrokeIndex, -1,
        &cwSketchPainterPathModel::activeStrokeIndexChanged);

    QPointer<QAbstractItemModel> m_strokeModel;
    Path m_activePath;
    QList<Path> m_finishedPaths;

    int m_previousActiveStroke = -1;

    double m_maxHalfWidth = 3.0;
    double m_minHalfWidth = 0.25;
    double m_widthScale   = 1.5;
    int    m_endPointTessellation = 5;

    static constexpr int m_finishLineIndexOffset = 1;

    // Roles are resolved by name so a proxy model inserted between cwPenStrokeModel
    // and this model still works (matches the planned cwMovingAveragePenStrokeProxy).
    int m_pointsRole    = -1;
    int m_brushNameRole = -1;
    void resolveRoles();

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

    void onSourceRowsInserted(const QModelIndex &parent, int first, int last);
    void onSourceRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last);
    void onSourceDataChanged(const QModelIndex &tl, const QModelIndex &br,
                             const QList<int> &roles);
    void onSourceModelReset();
    void onActiveStrokeIndexChanged();

    void updateActivePath();
    void rebuildAllFinished();
    void addOrUpdateFinishPath(int sourceRow);
    void clearFinished();

    void buildStrokeGeometry(QPainterPath &out, int sourceRow) const;
};

#endif // CWSKETCHPAINTERPATHMODEL_H
