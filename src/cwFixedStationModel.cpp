#include "cwFixedStationModel.h"

cwFixedStationModel::cwFixedStationModel(QObject *parent) :
    cwGenericTableModel<cwFixedStation>(parent)
{
    auto station = addColumn("Station");
    station->registerDataRole(Qt::DisplayRole,
                              [](const QModelIndex& index, const cwFixedStation& station)
    {
        Q_UNUSED(index)
        return station.stationName();
    });
    station->registerSetDataRole(Qt::EditRole,
                                 [this](const QModelIndex& index, cwFixedStation& station, const QVariant& newName)
    {
        auto name = newName.toString();
        if(name != station.stationName()) {
            station.setStationName(name);
            emit dataChanged(index, index, {Qt::DisplayRole});
        }
        return true;
    });

    auto latitude = addColumn("Latitude");
    latitude->registerDataRole(Qt::DisplayRole,
                               [](const QModelIndex& index, const cwFixedStation& station)
    {
        Q_UNUSED(index);
        return station.latitude();
    });

    latitude->registerSetDataRole(Qt::EditRole,
                               [this](const QModelIndex& index, cwFixedStation& station, const QVariant& value)
    {
        auto latStr = value.toString();
        if(latStr != station.latitude()) {
            station.setLatitude(latStr);
            emit dataChanged(index, index, {Qt::DisplayRole});
        }
        return true;
    });

    auto longitude = addColumn("Longitude");
    longitude->registerDataRole(Qt::DisplayRole,
                               [](const QModelIndex& index, const cwFixedStation& station)
    {
        Q_UNUSED(index);
        return station.longitude();
    });

    longitude->registerSetDataRole(Qt::EditRole,
                               [this](const QModelIndex& index, cwFixedStation& station, const QVariant& value)
    {
        auto str = value.toString();
        if(str != station.longitude()) {
            station.setLongitude(str);
            emit dataChanged(index, index, {Qt::DisplayRole});
        }
        return true;
    });

    auto altitude = addColumn("Altitude");
    altitude->registerDataRole(Qt::DisplayRole,
                                [](const QModelIndex& index, const cwFixedStation& station)
    {
        Q_UNUSED(index);
        return station.altitude();
    });

    altitude->registerSetDataRole(Qt::EditRole,
                                   [this](const QModelIndex& index, cwFixedStation& station, const QVariant& value)
    {
        auto str = value.toString();
        if(str != station.altitude()) {
            station.setAltitude(str);
            emit dataChanged(index, index, {Qt::DisplayRole});
        }
        return true;
    });
}

cwFixedStation cwFixedStationModel::fixedStation(const QString &stationName,
                                                 const QString &latitude,
                                                 const QString &longitude,
                                                 const QString &altitude) const
{
    cwFixedStation station;
    station.setStationName(stationName);
    station.setLatitude(latitude);
    station.setLongitude(longitude);
    station.setAltitude(altitude);
    return station;
}
