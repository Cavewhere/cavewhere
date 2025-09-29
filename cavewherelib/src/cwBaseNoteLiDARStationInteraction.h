#ifndef CWBASENOTELIDARSTATIONINTERACTION_H
#define CWBASENOTELIDARSTATIONINTERACTION_H

#include "cwInteraction.h"
#include "cwNoteLiDAR.h"

class cwBaseNoteLiDARStationInteraction : public cwInteraction
{
    Q_OBJECT
    QML_NAMED_ELEMENT(BaseNoteLiDARStationInteraction)

public:
    cwBaseNoteLiDARStationInteraction(QQuickItem* parent = nullptr);

    Q_INVOKABLE void addPoint(QVector3D position, cwNoteLiDAR* noteLiDAR);
};

#endif // CWBASENOTELIDARSTATIONINTERACTION_H
