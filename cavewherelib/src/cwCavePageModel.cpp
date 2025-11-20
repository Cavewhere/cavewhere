#include "cwCavePageModel.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwTripLengthTask.h"
#include "cwUsedStationTaskManager.h"
#include "cwErrorListModel.h"

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

    auto destroyTripData = [this](const TripData& data) {
        data.lengthTask->disconnect(this);
        data.lengthTask->deleteLater();
        data.usedStationsManager->disconnect(this);
        data.usedStationsManager->deleteLater();
        if(data.trip) {
            data.trip->disconnect(this);
            data.trip->errorModel()->disconnect(this);
        }
    };

    // Disconnect previous tasks and clear data
    for (const TripData &data : m_tripDataList) {
        destroyTripData(data);
    }
    m_tripDataList.clear();

    m_cave = cave;

    // Lambda to add a trip and connect necessary signals
    auto addTrip = [this](cwTrip* trip) {
        TripData tripData;
        tripData.trip = trip;

        // Length task
        cwTripLengthTask* lengthTask = new cwTripLengthTask(this);
        tripData.lengthTask = lengthTask;

        // Used stations manager
        cwUsedStationTaskManager* usedStationsManager = new cwUsedStationTaskManager(this);
        usedStationsManager->setAbbreviated(true);
        usedStationsManager->setBold(false);
        usedStationsManager->setOnlyLargestRange(true);
        tripData.usedStationsManager = usedStationsManager;

        // Add trip data to the list
        m_tripDataList.append(tripData);

        // Lambda to get the current index of the trip
        auto tripIndex = [this, trip]()->int {
            if(m_cave) {
                return m_cave->trips().indexOf(trip);
            } else {
                return -1;
            }
        };


        // Connect length task to update TripDistanceRole
        connect(lengthTask, &cwTripLengthTask::finished, this, [this, tripIndex]() {
            int row = tripIndex();
            if (row >= 0 && row < m_tripDataList.size()) {
                TripData &data = m_tripDataList[row];
                data.length = data.lengthTask->length();
                QModelIndex idx = index(row);
                emit dataChanged(idx, idx, {TripDistanceRole});
            }
        });

        // Connect used stations manager to update UsedStationsRole
        connect(usedStationsManager, &cwUsedStationTaskManager::usedStationsChanged, this, [this, tripIndex]() {
            int row = tripIndex();
            if (row >= 0 && row < m_tripDataList.size()) {
                TripData &data = m_tripDataList[row];
                data.usedStations = data.usedStationsManager->usedStations();
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

        auto dataChangedForErrorCount = [this, tripIndex]() {
            int i = tripIndex();
            Q_ASSERT(i >= 0);
            QModelIndex idx = index(i);
            emit dataChanged(idx, idx, {ErrorCountRole});
        };

        connect(trip->errorModel(), &cwErrorModel::warningCountChanged, this, dataChangedForErrorCount);
        connect(trip->errorModel(), &cwErrorModel::fatalCountChanged, this, dataChangedForErrorCount);

        lengthTask->setTrip(trip);
        usedStationsManager->setTrip(trip);
    };

    if (m_cave) {
        const QList<cwTrip*> trips = m_cave->trips();
        m_tripDataList.reserve(trips.size());

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
        connect(m_cave, &cwCave::beginRemoveTrips, this, [this, destroyTripData](int begin, int end) {
            beginRemoveRows(QModelIndex(), begin, end);
            for (int i = end; i >= begin; --i) {
                destroyTripData(m_tripDataList[i]);
                m_tripDataList.removeAt(i);
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
    for(const auto& tripData : m_tripDataList) {
        tripData.usedStationsManager->waitForFinished();
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
    if (row < 0 || row >= m_tripDataList.size())
        return QVariant();

    const TripData &tripData = m_tripDataList.at(row);
    cwTrip* trip = tripData.trip;

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
        return tripData.usedStations;
    case TripDistanceRole:
        return tripData.length;
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
        {TripDistanceRole, "tripDistanceRole"}
    };
}
