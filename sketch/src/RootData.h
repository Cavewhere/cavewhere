#pragma once

//Qt inculdes
#include <QObject>
#include <QQmlEngine>

//CaveWhere includes
#include "cwProject.h"
#include "cwTrip.h"
#include "cwCavingRegion.h"

//Sketch includes
#include "CenterlinePainterModel.h"
#include "PenLineModel.h"
#include "RepositoryModel.h"

//QQuickGit inculdes
#include "Account.h"
#include "AccountSettingWatcher.h"

namespace cwSketch {

class RootData : public QObject
{
    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(RootDataSketch)

    Q_PROPERTY(RepositoryModel* repositoryModel READ repositoryModel CONSTANT)
    Q_PROPERTY(cwProject* project READ project CONSTANT)
    Q_PROPERTY(cwCavingRegion* cavingRegion READ cavingRegion NOTIFY cavingRegionChanged)

    Q_PROPERTY(QQuickGit::Account* account READ account CONSTANT)

    //MSAA sample count
    Q_PROPERTY(int sampleCount READ sampleCount CONSTANT FINAL)

    //For testing
    Q_PROPERTY(cwTrip* currentTrip READ currentTrip CONSTANT)
    Q_PROPERTY(CenterlinePainterModel* centerlinePainterModel READ centerlinePainterModel CONSTANT)
    Q_PROPERTY(PenLineModel* penLineModel READ penLineModel CONSTANT)

public:
    RootData(QObject* parent = nullptr);

    RootData(const RootData &) = delete;
    RootData(RootData &&) = delete;
    RootData &operator=(const RootData &) = delete;
    RootData &operator=(RootData &&) = delete;

    cwProject *project() const { return m_project; }
    cwCavingRegion* cavingRegion() const { return m_project->cavingRegion(); }


    //For testing
    cwTrip* currentTrip() const { return m_currentTrip; }

    CenterlinePainterModel* centerlinePainterModel() const
    {
        return m_centerlinePainterModel;
    }

    PenLineModel* penLineModel() const {
        return m_penLineModel;
    }

    int sampleCount() const;


    QQuickGit::Account *account() const;

    RepositoryModel *repositoryModel() const;

signals:
    void cavingRegionChanged();

private:
    cwProject* m_project;
    QQuickGit::Account* m_account;
    RepositoryModel* m_repositoryModel;

    int m_sampleCount = 1;

    //internal
    QQuickGit::AccountSettingWatcher* m_accountWatcher;

    //For testing
    QPointer<cwTrip> m_currentTrip;

    void createCurrentTrip();
    void createGeometry2DPipeline();

    CenterlinePainterModel* m_centerlinePainterModel;
    PenLineModel* m_penLineModel;
};

}
