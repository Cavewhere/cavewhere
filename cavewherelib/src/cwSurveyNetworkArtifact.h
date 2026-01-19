// cwSurveyNetworkArtifact.h
#ifndef CWSURVEYNETWORKARTIFACT_H
#define CWSURVEYNETWORKARTIFACT_H

//Qt includes
#include <QFuture>

//Our includes
#include "cwArtifact.h"
#include "Monad/Monad.h"
#include "cwSurveyNetwork.h"
#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwSurveyNetworkArtifact : public cwArtifact
{
    Q_OBJECT
    Q_PROPERTY(QFuture<Monad::Result<cwSurveyNetwork>> surveyNetwork READ surveyNetwork WRITE setSurveyNetwork NOTIFY surveyNetworkChanged)

public:
    explicit cwSurveyNetworkArtifact(QObject *parent = nullptr)
        : cwArtifact(parent) {}

    QFuture<Monad::Result<cwSurveyNetwork>> surveyNetwork() const { return m_surveyNetwork; }
    void setSurveyNetwork(const QFuture<Monad::Result<cwSurveyNetwork>>& surveyNetwork)
    {
        m_surveyNetwork = surveyNetwork;
        emit surveyNetworkChanged();
    }

signals:
    void surveyNetworkChanged();

private:
    QFuture<Monad::Result<cwSurveyNetwork>> m_surveyNetwork;
};

#endif // CWSURVEYNETWORKARTIFACT_H
