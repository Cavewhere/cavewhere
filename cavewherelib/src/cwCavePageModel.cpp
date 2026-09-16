#include "cwCavePageModel.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwTripStatsWatcher.h"
#include "cwErrorListModel.h"
#include "cwTripCalibration.h"

cwCavePageModel::cwCavePageModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

cwCavePageModel::~cwCavePageModel()
{

    waitForFinished(); //This prevents crashing if cwCavePageModel deleted
}

cwCave* cwCavePageModel::cave() const
{
    return m_cave;
}

void cwCavePageModel::setCave(cwCave* cave)
{
    if (m_cave == cave)
        return;

    beginResetModel();

    auto destroyTripStats = [this](cwTripStatsWatcher* stats) {
        stats->disconnect(this);
        stats->deleteLater();
        if(cwTrip* trip = stats->trip()) {
            trip->disconnect(this);
            trip->errorModel()->disconnect(this);
            trip->calibrations()->disconnect(this);
        }
    };

    // Disconnect previous tasks and clear data
    for (cwTripStatsWatcher* stats : std::as_const(m_tripStats)) {
        destroyTripStats(stats);
    }
    m_tripStats.clear();

    m_cave = cave;

    // Lambda to add a trip and connect necessary signals
    auto addTrip = [this](cwTrip* trip) {
        // The trip's length and used stations, the same mechanism the survey
        // tree reads them through
        cwTripStatsWatcher* stats = new cwTripStatsWatcher(trip, this);
        m_tripStats.append(stats);

        // Lambda to get the current index of the trip
        auto tripIndex = [this, trip]()->int {
            if(m_cave) {
                return m_cave->trips().indexOf(trip);
            } else {
                return -1;
            }
        };


        // Connect the watcher's length to update TripDistanceRole
        connect(stats, &cwTripStatsWatcher::lengthChanged, this, [this, tripIndex]() {
            int row = tripIndex();
            if (row >= 0 && row < m_tripStats.size()) {
                QModelIndex idx = index(row);
                emit dataChanged(idx, idx, {TripDistanceRole});
            }
        });

        // Connect the watcher's used stations to update UsedStationsRole
        connect(stats, &cwTripStatsWatcher::usedStationsChanged, this, [this, tripIndex]() {
            int row = tripIndex();
            if (row >= 0 && row < m_tripStats.size()) {
                QModelIndex idx = index(row);
                emit dataChanged(idx, idx, {UsedStationsRole});
            }
        });

        // Connect trip's properties for name, date, and error count
        connect(trip, &cwTrip::nameChanged, this, [this, tripIndex]() {
            int row = tripIndex();
            if (row >= 0) {
                QModelIndex idx = index(row);
                emit dataChanged(idx, idx, {TripNameRole});
            }
        });

        connect(trip, &cwTrip::dateChanged, this, [this, tripIndex]() {
            int row = tripIndex();
            if (row >= 0) {
                QModelIndex idx = index(row);
                emit dataChanged(idx, idx, {TripDateRole});
            }
        });

        auto emitRoleChanged = [this, tripIndex](int role) {
            int row = tripIndex();
            if (row >= 0) {
                QModelIndex idx = index(row);
                emit dataChanged(idx, idx, {role});
            }
        };

        auto emitErrorCountChanged = [emitRoleChanged]() { emitRoleChanged(ErrorCountRole); };
        connect(trip->errorModel(), &cwErrorModel::warningCountChanged, this, emitErrorCountChanged);
        connect(trip->errorModel(), &cwErrorModel::fatalCountChanged, this, emitErrorCountChanged);

        auto emitAutoDeclinationChanged = [emitRoleChanged]() { emitRoleChanged(AutoDeclinationRole); };
        auto* calibration = trip->calibrations();
        connect(calibration, &cwTripCalibration::declinationChanged, this, [emitRoleChanged]() {
            emitRoleChanged(DeclinationRole);
        });
        connect(calibration, &cwTripCalibration::autoDeclinationChanged, this, emitAutoDeclinationChanged);
        connect(calibration, &cwTripCalibration::autoDeclinationAvailableChanged, this, emitAutoDeclinationChanged);
    };

    if (m_cave) {
        const QList<cwTrip*> trips = m_cave->trips();
        m_tripStats.reserve(trips.size());

        // Add each trip initially
        for (cwTrip* trip : trips) {
            addTrip(trip);
        }

        // Connect to handle dynamically adding trips
        connect(m_cave, &cwCave::beginInsertTrips, this, [this](int begin, int end) {
            beginInsertRows(QModelIndex(), begin, end);
        });
        connect(m_cave, &cwCave::insertedTrips, this, [this, addTrip](int begin, int end) {
            for (int i = begin; i <= end; ++i) {
                addTrip(m_cave->trips().at(i));
            }
            endInsertRows();
        });

        // Connect to handle dynamically removing trips
        connect(m_cave, &cwCave::beginRemoveTrips, this, [this, destroyTripStats](int begin, int end) {
            beginRemoveRows(QModelIndex(), begin, end);
            for (int i = end; i >= begin; --i) {
                destroyTripStats(m_tripStats.at(i));
                m_tripStats.removeAt(i);
            }
        });
        connect(m_cave, &cwCave::removedTrips, this, [this](int begin, int end) {
            endRemoveRows();
        });
    }

    endResetModel();

    emit caveChanged();
}



//Useful for testcases
void cwCavePageModel::waitForFinished()
{
    for(cwTripStatsWatcher* stats : std::as_const(m_tripStats)) {
        stats->waitForFinished();
    }
}

int cwCavePageModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    if (!m_cave) {
        return 0;
    }

    return m_cave->trips().count();
}

int cwCavePageModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QModelIndex cwCavePageModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    if (!m_cave || row < 0 || column != 0 || row >= rowCount()) {
        return QModelIndex();
    }

    return createIndex(row, column);
}

QModelIndex cwCavePageModel::parent(const QModelIndex &child) const
{
    Q_UNUSED(child)
    return QModelIndex();
}

QVariant cwCavePageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !m_cave)
        return QVariant();

    int row = index.row();
    if (row < 0 || row >= m_tripStats.size())
        return QVariant();

    const cwTripStatsWatcher* stats = m_tripStats.at(row);
    cwTrip* trip = stats->trip();

    switch (role) {
    case TripObjectRole:
        return QVariant::fromValue(trip);
    case ErrorCountRole: {
        auto errorModel = trip->errorModel();

        // for(auto childModel : errorModel->childModels()) {
        //     for(auto child : childModel->childModels()) {
        //         auto errors = child->errors()->toList();
        //         qDebug() << "Errors:" << child << trip->name() << errors.size();
        //         for(const auto& error : std::as_const(errors)) {
        //             qDebug() << "Error:" << trip->name() << error.message();
        //         }
        //     }
        // }

        return errorModel->fatalCount() + errorModel->warningCount();
    }
    case TripNameRole:
        return trip->name();
    case TripDateRole:
        return trip->date();
    case UsedStationsRole:
        return stats->usedStations();
    case TripDistanceRole:
        return stats->length();
    case DeclinationRole:
        return trip->calibrations()->declination();
    case AutoDeclinationRole:
        return trip->calibrations()->autoDeclination()
               && trip->calibrations()->autoDeclinationAvailable();
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> cwCavePageModel::roleNames() const
{
    return {
        {TripObjectRole, "tripObjectRole"},
        {ErrorCountRole, "errorCountRole"},
        {TripNameRole, "tripNameRole"},
        {TripDateRole, "tripDateRole"},
        {UsedStationsRole, "usedStationsRole"},
        {TripDistanceRole, "tripDistanceRole"},
        {DeclinationRole, "declinationRole"},
        {AutoDeclinationRole, "autoDeclinationRole"}
    };
}
