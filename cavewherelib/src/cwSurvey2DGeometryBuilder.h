// cwSurvey2DGeometryBuilder.h
#ifndef CWSURVEY2DGEOMETRYBUILDER_H
#define CWSURVEY2DGEOMETRYBUILDER_H

//Our includes
#include "cwSurveyNetworkSource.h"
#include "cwSurvey2DGeometrySource.h"
#include "CaveWhereLibExport.h"

//Qt includes
#include <QMatrix4x4>
#include <QObject>
#include <QPointer>

class CAVEWHERE_LIB_EXPORT cwSurvey2DGeometryBuilder : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cwSurveyNetworkSource* surveyNetwork READ surveyNetwork WRITE setSurveyNetwork NOTIFY surveyNetworkChanged)
    Q_PROPERTY(QMatrix4x4 viewMatrix READ viewMatrix WRITE setViewMatrix NOTIFY viewMatrixChanged)
    Q_PROPERTY(cwSurvey2DGeometrySource* survey2DGeometry READ survey2DGeometry CONSTANT)

public:
    explicit cwSurvey2DGeometryBuilder(QObject* parent = nullptr);

    cwSurveyNetworkSource* surveyNetwork() const;
    void setSurveyNetwork(cwSurveyNetworkSource* surveyNetwork);

    QMatrix4x4 viewMatrix() const;
    void setViewMatrix(const QMatrix4x4& matrix);

    cwSurvey2DGeometrySource* survey2DGeometry() const;

signals:
    void surveyNetworkChanged();
    void viewMatrixChanged();

private:
    void updateGeometry();

    QPointer<cwSurveyNetworkSource> m_surveyNetwork;
    QMatrix4x4 m_viewMatrix;
    cwSurvey2DGeometrySource* m_survey2DGeometry;
};

#endif // CWSURVEY2DGEOMETRYBUILDER_H
