#ifndef CWFIXEDSTATIONMODEL_H
#define CWFIXEDSTATIONMODEL_H

//Our includes
#include "QQmlGadgetListModel.h"
#include "cwFixedStation.h"

class cwFixedStationModel : public QQmlGadgetListModel<cwFixedStation>
{
    Q_OBJECT
public:
    cwFixedStationModel(QObject* parent);

};

#endif // CWFIXEDSTATIONMODEL_H
