// CenterlinePainterModel.h
#pragma once

#include "AbstractPainterPathModel.h"
#include "cwSurvey2DGeometrySource.h"

//Qt includes
#include <QVector>
#include <QPointer>

namespace cwSketch {

class CenterlinePainterModel : public AbstractPainterPathModel
{
    Q_OBJECT
    Q_PROPERTY(cwSurvey2DGeometrySource* survey2DGeometry READ survey2DGeometry WRITE setSurvey2DGeometry NOTIFY survey2DGeometryChanged)

public:
    explicit CenterlinePainterModel(QObject *parent = nullptr);

    cwSurvey2DGeometrySource* survey2DGeometry() const;
    void setSurvey2DGeometry(cwSurvey2DGeometrySource* geometry);

    // ListModel overrides
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

signals:
    void survey2DGeometryChanged();

private slots:
    void updateModel();

private:
    // implement base’s pure virtual
    Path path(const QModelIndex &index) const override;

    QPointer<cwSurvey2DGeometrySource> m_geometrySource;

    QVector<Path> m_paths;
};

} // namespace cwSketch
