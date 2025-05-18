// cwSurvey2DGeometry.h
#ifndef CWSURVEY2DGEOMETRY_H
#define CWSURVEY2DGEOMETRY_H

#include <QLineF>
#include <QVector>
#include <QPointF>
#include <QString>

struct Station2DGeometry {
    QString name;
    QPointF position;
};

struct cwSurvey2DGeometry {
    QVector<QLineF> shotLines;
    QVector<Station2DGeometry> stations;
};

#endif // CWSURVEY2DGEOMETRY_H
