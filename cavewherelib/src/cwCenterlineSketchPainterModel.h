/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCENTERLINESKETCHPAINTERMODEL_H
#define CWCENTERLINESKETCHPAINTERMODEL_H

//Qt includes
#include <QPointer>
#include <QQmlEngine>
#include <QVector>

//Our includes
#include "cwAbstractSketchPainterPathModel.h"
#include "cwGridTextModel.h"
#include "cwScale.h"
#include "cwSurvey2DGeometryArtifact.h"
#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwCenterlineSketchPainterModel : public cwAbstractSketchPainterPathModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CenterlineSketchPainterModel)

    Q_PROPERTY(cwSurvey2DGeometryArtifact* survey2DGeometry READ survey2DGeometry WRITE setSurvey2DGeometry NOTIFY survey2DGeometryChanged)
    // Map scale (e.g. 1:250) used to size station markers + labels in
    // paper-millimetres. Symbols stay the same size on paper across
    // different map scales. Positions and shot-line widths are in world
    // meters — those scale with mapScale and user zoom.
    Q_PROPERTY(cwScale* mapScale READ mapScale WRITE setMapScale NOTIFY mapScaleChanged)

public:
    explicit cwCenterlineSketchPainterModel(QObject *parent = nullptr);

    cwSurvey2DGeometryArtifact *survey2DGeometry() const { return m_geometryArtifact; }
    void setSurvey2DGeometry(cwSurvey2DGeometryArtifact *geometry);

    cwScale *mapScale() const { return m_mapScale; }
    void setMapScale(cwScale *scale);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    // Station labels are emitted as text rows (not baked glyph paths) so the
    // backend draws real text — this keeps character counters ('8', '0', 'e')
    // hollow on the Canvas backend (which lacks a path fill-rule API) and lets
    // SVG/PDF export produce selectable <text> elements.
    QVector<cwGridTextModel::TextRow> textRows() const { return m_textRows; }

signals:
    void survey2DGeometryChanged();
    void mapScaleChanged();
    void textRowsChanged();

protected:
    Path path(const QModelIndex &index) const override;

private slots:
    void updateModel();

private:
    QPointer<cwSurvey2DGeometryArtifact> m_geometryArtifact;
    QPointer<cwScale> m_mapScale;
    QVector<Path> m_paths;
    QVector<cwGridTextModel::TextRow> m_textRows;
};

#endif // CWCENTERLINESKETCHPAINTERMODEL_H
