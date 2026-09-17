// cwSurveyNetworkSource.h
#ifndef CWSURVEYNETWORKSOURCE_H
#define CWSURVEYNETWORKSOURCE_H

//Qt includes
#include <QFuture>
#include <QObject>

//Our includes
#include "Monad/Monad.h"
#include "cwSurveyNetwork.h"
#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwSurveyNetworkSource : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QFuture<Monad::Result<cwSurveyNetwork>> surveyNetwork READ surveyNetwork WRITE setSurveyNetwork NOTIFY surveyNetworkChanged)

public:
    explicit cwSurveyNetworkSource(QObject *parent = nullptr)
        : QObject(parent) {}

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

#endif // CWSURVEYNETWORKSOURCE_H
