#ifndef CWFIXEDSTATIONMODEL_H
#define CWFIXEDSTATIONMODEL_H

//Our includes
#include "cwGenericTableModel.h"
#include "cwFixedStation.h"

class cwFixedStationModel : public cwGenericTableModel<cwFixedStation>
{
    Q_OBJECT
public:
    enum Roles {
        UnitRole = Qt::UserRole
    };
 
    enum Column {
        
    };
    
    cwFixedStationModel(QObject* parent = nullptr);

    //Need for QML creation of fixedStation
    Q_INVOKABLE cwFixedStation fixedStation(const QString& stationName,
                                            const QString& latitude,
                                            const QString& longitude,
                                            const QString& altitude) const;
};

#endif // CWFIXEDSTATIONMODEL_H
