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

namespace cwSketch {

class RootData : public QObject
{
    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(RootDataSketch)

    Q_PROPERTY(cwProject* project READ project CONSTANT)
    Q_PROPERTY(cwCavingRegion* cavingRegion READ cavingRegion NOTIFY cavingRegionChanged)

    //For testing
    Q_PROPERTY(cwTrip* currentTrip READ currentTrip CONSTANT)
    Q_PROPERTY(CenterlinePainterModel* centerlinePainterModel READ centerlinePainterModel CONSTANT)


public:
    RootData(QObject* parent = nullptr);

    cwProject* project() const { return m_project; }
    cwCavingRegion* cavingRegion() const { return m_project->cavingRegion(); }


    //For testing
    cwTrip* currentTrip() const { return m_currentTrip; }

    CenterlinePainterModel* centerlinePainterModel() const
    {
        return m_centerlinePainterModel;
    }

signals:
    void cavingRegionChanged();

private:
    cwProject* m_project;

    //For testing
    QPointer<cwTrip> m_currentTrip;

    void createCurrentTrip();
    void createGeometry2DPipeline();


    CenterlinePainterModel* m_centerlinePainterModel;
};

}
