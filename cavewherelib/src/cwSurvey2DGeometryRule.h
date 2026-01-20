// cwSurvey2DGeometryRule.h
#ifndef CWSURVEY2DGEOMETRYRULE_H
#define CWSURVEY2DGEOMETRYRULE_H

#include "cwAbstractRule.h"
#include "cwSurveyNetworkArtifact.h"
#include "cwMatrix4x4Artifact.h"
#include "cwSurvey2DGeometryArtifact.h"
#include "CaveWhereLibExport.h"

//Qt includes
#include <QQmlEngine>

class CAVEWHERE_LIB_EXPORT cwSurvey2DGeometryRule : public cwAbstractRule {
    Q_OBJECT
    QML_NAMED_ELEMENT(Survey2DGeometryRule)

    Q_PROPERTY(cwSurveyNetworkArtifact* surveyNetwork READ surveyNetwork WRITE setSurveyNetwork NOTIFY surveyNetworkChanged)
    Q_PROPERTY(cwMatrix4x4Artifact* viewMatrix READ viewMatrix WRITE setViewMatrix NOTIFY viewMatrixChanged)
    Q_PROPERTY(cwSurvey2DGeometryArtifact* survey2DGeometry READ survey2DGeometry CONSTANT)

public:
    explicit cwSurvey2DGeometryRule(QObject* parent = nullptr);

    cwSurveyNetworkArtifact* surveyNetwork() const;
    void setSurveyNetwork(cwSurveyNetworkArtifact*);

    cwMatrix4x4Artifact* viewMatrix() const;
    void setViewMatrix(cwMatrix4x4Artifact*);

    cwSurvey2DGeometryArtifact* survey2DGeometry() const;

protected:
    QList<QByteArray> sourcesNames() const override;
    QList<QByteArray> outputsNames() const override;

private slots:
    void updateGeometry();

private:
    QPointer<cwSurveyNetworkArtifact> m_surveyNetwork;
    QPointer<cwMatrix4x4Artifact> m_viewMatrix;
    cwSurvey2DGeometryArtifact* m_survey2DGeometry;

signals:
    void surveyNetworkChanged();
    void viewMatrixChanged();
    void survey2DGeometryChanged();
};

#endif // CWSURVEY2DGEOMETRYRULE_H
