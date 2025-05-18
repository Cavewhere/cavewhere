// CenterlinePainterModel.h
#pragma once

#include "AbstractPainterPathModel.h"
#include "cwSurvey2DGeometryArtifact.h"

//Qt includes
#include <QVector>
#include <QPointer>

namespace cwSketch {

class CenterlinePainterModel : public AbstractPainterPathModel
{
    Q_OBJECT
    Q_PROPERTY(cwSurvey2DGeometryArtifact* survey2DGeometry READ survey2DGeometry WRITE setSurvey2DGeometry NOTIFY survey2DGeometryChanged)

public:
    explicit CenterlinePainterModel(QObject *parent = nullptr);

    cwSurvey2DGeometryArtifact* survey2DGeometry() const;
    void setSurvey2DGeometry(cwSurvey2DGeometryArtifact* geometry);

    // ListModel overrides
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

signals:
    void survey2DGeometryChanged();

private slots:
    void updateModel();

private:
    // implement base’s pure virtual
    const Path& path(const QModelIndex &index) const override;

    QPointer<cwSurvey2DGeometryArtifact> m_geometryArtifact;

    QVector<Path> m_paths;
};

} // namespace cwSketch
