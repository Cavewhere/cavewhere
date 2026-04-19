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
#include "cwPenPoint.h"

class CAVEWHERE_LIB_EXPORT cwSketchPainterPathModel : public cwAbstractSketchPainterPathModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SketchPainterPathModel)

    Q_PROPERTY(QAbstractItemModel *strokeModel READ strokeModel WRITE setStrokeModel NOTIFY strokeModelChanged)
    Q_PROPERTY(int activeStrokeIndex READ activeStrokeIndex WRITE setActiveStrokeIndex NOTIFY activeStrokeIndexChanged BINDABLE bindableActiveStrokeIndex)

public:
    explicit cwSketchPainterPathModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QAbstractItemModel *strokeModel() const;
    void setStrokeModel(QAbstractItemModel *model);

    int activeStrokeIndex() const { return m_activeStrokeIndex.value(); }
    void setActiveStrokeIndex(int index) { m_activeStrokeIndex = index; }
    QBindable<int> bindableActiveStrokeIndex() { return &m_activeStrokeIndex; }

signals:
    void strokeModelChanged();
    void activeStrokeIndexChanged();

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
    int m_pointsRole = -1;
    int m_widthRole  = -1;
    int m_colorRole  = -1;
    void resolveRoles();

    QVector<cwPenPoint> strokePoints(int row) const;
    double              strokeWidth(int row) const;
    QColor              strokeColor(int row) const;

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

    double pressureToLineHalfWidth(const cwPenPoint &p) const;
    QLineF perpendicularLineAt(const QVector<cwPenPoint> &points, int index) const;
    QVector<QPointF> capFromNormal(const QPointF &normal, const QLineF &perpendicularLine,
                                   double radius) const;
    QVector<QPointF> endCap(const QVector<cwPenPoint> &points, const QLineF &perpendicularLine) const;

};

#endif // CWSKETCHPAINTERPATHMODEL_H
