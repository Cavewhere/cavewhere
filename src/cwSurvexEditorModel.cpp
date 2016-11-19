//Our includes
#include "cwSurvexEditorModel.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwTripCalibration.h"

cwSurvexEditorModel::cwSurvexEditorModel()
{

}

/**
* @brief cwSurvexEditorModel::setTrip
* @param trip -
*/
void cwSurvexEditorModel::setTrip(cwTrip* trip) {
    if(Trip != trip) {
        beginResetModel();
        Trip = trip;
        updateIndexToChunk();
        endResetModel();

        emit tripChanged();
    }
}

QVariant cwSurvexEditorModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid()) {
        return QVariant();
    }

    auto chunkIndex = modelIndexToStationChunk(index);
    if(chunkIndex.first == nullptr) {
        return QVariant();
    }

    Q_ASSERT(chunkIndex.second >= 0);

    cwSurveyChunk* chunk = chunkIndex.first;
    int stationIndex = chunkIndex.second;

    switch(role) {
    case StationNameRole:
        return QVariant(chunk->station(stationIndex).name());
    case StationLeftRole:
        return QVariant(chunk->station(stationIndex).left());
    case StationRightRole:
        return QVariant(chunk->station(stationIndex).right());
    case StationUpRole:
        return QVariant(chunk->station(stationIndex).up());
    case StationDownRole:
        return QVariant(chunk->station(stationIndex).down());
    default:
        break;
    }

    int shotIndex = stationIndex;
    if(shotIndex < chunk->shotCount()) {
        switch(role) {
        case ShotDistanceRole:
            return QVariant(chunk->shot(shotIndex).distance());
        case ShotCompassRole:
            return QVariant(chunk->shot(shotIndex).compass());
        case ShotClinoRole:
            return QVariant(chunk->shot(shotIndex).clino());
        case ShotCalibrationRole: {
            cwTripCalibration* calibration = chunk->calibrations().value(shotIndex);
            if(calibration != nullptr) {
                return QVariant::fromValue(calibration);
            }
        }
        default:
            break;
        }
    }

    return QVariant();
}

/**
 * @brief cwSurvexEditorModel::rowCount
 * @param parent
 * @return
 */
int cwSurvexEditorModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);

    if(Trip.isNull()) {
        return 0;
    }

    if(!IndexToChunk.isEmpty()) {
        auto lastIter = IndexToChunk.end() - 1;
        return lastIter.key().high();
    }
    return 0;
}

/**
 * @brief cwSurvexEditorModel::roleNames
 * @return
 */
QHash<int, QByteArray> cwSurvexEditorModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(StationNameRole, "stationName");
    roles.insert(StationLeftRole, "stationLeft");
    roles.insert(StationRightRole, "stationRight");
    roles.insert(StationUpRole, "stationUp");
    roles.insert(StationDownRole, "stationDown");
    roles.insert(ShotDistanceRole, "shotDistance");
    roles.insert(ShotCompassRole, "shotCompass");
    roles.insert(ShotBackCompassRole, "shotBackCompass");
    roles.insert(ShotClinoRole, "shotClino");
    roles.insert(ShotBackClinoRole, "shotBackClino");
    roles.insert(ShotCalibrationRole, "shotCalibration");
    return roles;
}

/**
 * @brief cwSurvexEditorModel::addShotCalibration
 * @param index - The index for the whole trip
 */
void cwSurvexEditorModel::addShotCalibration(int index)
{
    auto chunkIndex = indexToStationChunk(index);
    if(chunkIndex.first != nullptr) {
        chunkIndex.first->addCalibration(chunkIndex.second);

        QModelIndex dataChangeIndex = this->index(index);

        emit dataChanged(dataChangeIndex, dataChangeIndex, QVector<int>() << ShotCalibrationRole);
    }
}

/**
 * @brief cwSurvexEditorModel::updateIndexToChunk
 *
 * This clears the current IndexToChunk and updates it with a new lookup
 */
void cwSurvexEditorModel::updateIndexToChunk()
{
   IndexToChunk.clear();

   if(Trip.isNull()) {
       return;
   }

   int stationCount = 0;
   foreach(cwSurveyChunk* chunk, Trip->chunks()) {
       int begin = stationCount;
       int end = begin + chunk->stationCount() - 1;
       qDebug() << "Insert:" << chunk << begin << end;
       IndexToChunk.insert(Range(begin, end), chunk);
       stationCount += chunk->stationCount() + 1;
   }
}

/**
 * @brief cwSurvexEditorModel::modelIndexToStationChunk
 * @param index - The index from the model
 * @return The station index in the chunk and the chunk at index
 */
QPair<cwSurveyChunk*, int> cwSurvexEditorModel::modelIndexToStationChunk(const QModelIndex& index) const
{
    return indexToStationChunk(index.row());
}

/**
 * @brief cwSurvexEditorModel::indexToStationChunk
 * @param index - The row in model
 * @return The station index in teh chunk and teh cuhnk at index
 */
QPair<cwSurveyChunk*, int> cwSurvexEditorModel::indexToStationChunk(int index) const
{
    auto iter = IndexToChunk.lowerBound(Range(index));

    if(iter != IndexToChunk.end() && iter.key().low() <= index) {
        int realIndex = index - iter.key().low();
        cwSurveyChunk* chunk = iter.value();
        Q_ASSERT(realIndex < chunk->stationCount());
        Q_ASSERT(realIndex >= 0);
        return QPair<cwSurveyChunk*, int>(chunk, realIndex);
    }

    return QPair<cwSurveyChunk*, int>(nullptr, -1);
}
