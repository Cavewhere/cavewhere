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
#include "cwSurvey2DGeometryArtifact.h"
#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwCenterlineSketchPainterModel : public cwAbstractSketchPainterPathModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CenterlineSketchPainterModel)

    Q_PROPERTY(cwSurvey2DGeometryArtifact* survey2DGeometry READ survey2DGeometry WRITE setSurvey2DGeometry NOTIFY survey2DGeometryChanged)

public:
    explicit cwCenterlineSketchPainterModel(QObject *parent = nullptr);

    cwSurvey2DGeometryArtifact *survey2DGeometry() const { return m_geometryArtifact; }
    void setSurvey2DGeometry(cwSurvey2DGeometryArtifact *geometry);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

signals:
    void survey2DGeometryChanged();

protected:
    Path path(const QModelIndex &index) const override;

private slots:
    void updateModel();

private:
    QPointer<cwSurvey2DGeometryArtifact> m_geometryArtifact;
    QVector<Path> m_paths;
};

#endif // CWCENTERLINESKETCHPAINTERMODEL_H
