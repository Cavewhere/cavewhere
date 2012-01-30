#ifndef CWLINEPLOTGEOMETRYTASK_H
#define CWLINEPLOTGEOMETRYTASK_H

//Our includes
#include "cwTask.h"
#include "cwStation.h"
class cwCavingRegion;
class cwCave;


//Qt includes
#include <QVector>
#include <QVector3D>
#include <QWeakPointer>
#include <QMap>

/**
  \brief This class isn't thread safe!
  */
class cwLinePlotGeometryTask : public cwTask
{
    Q_OBJECT
public:
    explicit cwLinePlotGeometryTask(QObject *parent = 0);

    //Inputs
    void setRegion(cwCavingRegion* region);

    //Outputs
    QVector<QVector3D> pointData() const;
    QVector<unsigned int> indexData() const;

protected:
    void runTask();

signals:

public slots:

private:
    //Inputs
    cwCavingRegion* Region;

    //Outputs
    QVector<QVector3D> PointData;
    QVector<unsigned int> IndexData;

    //Lookup to look up the station and get it's index
    QMap< cwStation*, unsigned int > StationIndexLookup;

    void addStationPositions(cwCave* cave);
    void addShotLines(cwCave* cave);
};

/**
  \brief Sets the region that the task will generate the geometry for

  \param region - should be valid
  */
inline void cwLinePlotGeometryTask::setRegion(cwCavingRegion* region) {
    Region = region;
}

/**
  \brief Gets the point geometry for all the stations
  */
inline QVector<QVector3D> cwLinePlotGeometryTask::pointData() const {
    return PointData;
}

/**
  \brief Gets all the index data for creating the lines in opengl

  This index can be drawn in opengl using draw elements
  */
inline QVector<unsigned int> cwLinePlotGeometryTask::indexData() const {
    return IndexData;
}

#endif // CWLINEPLOTGEOMETRYTASK_H
