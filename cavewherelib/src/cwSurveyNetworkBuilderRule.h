// cwSurveyNetworkBuilderRule.h
#ifndef CWSURVEYNETWORKBUILDERRULE_H
#define CWSURVEYNETWORKBUILDERRULE_H

//Our includes
#include "cwAbstractRule.h"
#include "cwSurveyNetworkArtifact.h"
#include "cwSurveyDataArtifact.h"  // Assuming this class exists
#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwSurveyNetworkBuilderRule : public cwAbstractRule
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SurveyNetworkBuilderRule)

    // Sources
    Q_PROPERTY(cwSurveyDataArtifact* surveyData READ surveyData WRITE setSurveyData NOTIFY surveyDataChanged)

    // Outputs
    Q_PROPERTY(cwSurveyNetworkArtifact* surveyNetworkArtifact READ surveyNetworkArtifact CONSTANT)

public:
    explicit cwSurveyNetworkBuilderRule(QObject *parent = nullptr);

    // Getter and Setter for survey data
    cwSurveyDataArtifact* surveyData() const { return m_surveyData; }
    void setSurveyData(cwSurveyDataArtifact* surveyData);

    // Getter and Setter for survey network artifact
    cwSurveyNetworkArtifact* surveyNetworkArtifact() const { return m_surveyNetworkArtifact; }

    // Implementing abstract methods
    QList<QByteArray> sourcesNames() const override
    {
        return { "surveyData" };  // Return a list of sources names
    }

    QList<QByteArray> outputsNames() const override
    {
        return { "surveyNetworkArtifact" };  // Return a list of output names
    }

signals:
    void surveyDataChanged();

private slots:
    void updatePipeline();

private:
    //Source
    QPointer<cwSurveyDataArtifact> m_surveyData;

    //Output
    cwSurveyNetworkArtifact* m_surveyNetworkArtifact;
};

#endif // CWSURVEYNETWORKBUILDERRULE_H
