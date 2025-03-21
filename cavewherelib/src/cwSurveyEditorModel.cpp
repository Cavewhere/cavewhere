//Our includes
#include "cwSurveyEditorModel.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwTripCalibration.h"

cwSurveyEditorModel::cwSurveyEditorModel()
{

}

/**
* @brief cwSurveyEditorModel::setTrip
* @param trip -
*/
void cwSurveyEditorModel::setTrip(cwTrip* trip) {
    if(Trip != trip) {
        beginResetModel();
        Trip = trip;
        updateIndexToChunk();
        endResetModel();

        emit tripChanged();
    }
}

QVariant cwSurveyEditorModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid()) {
        return QVariant();
    }

    auto chunkIndex = modelIndexToStationChunk(index);
    if(chunkIndex.first == nullptr) {
        switch(role) {
        case StationVisibleRole:
        case ShotVisibleRole:
            return false;
        default:
            return QVariant();
        }
    }

    Q_ASSERT(chunkIndex.second >= 0);

    cwSurveyChunk* chunk = chunkIndex.first;
    int stationIndex = chunkIndex.second;

    switch(role) {
    case StationNameRole:
        return QVariant(chunk->station(stationIndex).name());
    case StationLeftRole:
        return QVariant(chunk->station(stationIndex).left().value());
    case StationRightRole:
        return QVariant(chunk->station(stationIndex).right().value());
    case StationUpRole:
        return QVariant(chunk->station(stationIndex).up().value());
    case StationDownRole:
        return QVariant(chunk->station(stationIndex).down().value());
    case ChunkRole:
        return QVariant::fromValue(chunk);
    case ChunkIdRole:
        return QString("%1").arg(reinterpret_cast<qlonglong>(chunk));
    case StationVisibleRole:
        return true;
    default:
        break;
    }

    int shotIndex = stationIndex;

    if(shotIndex < chunk->shotCount()) {
        switch(role) {
        case ShotDistanceRole:
            return QVariant(chunk->shot(shotIndex).distance().value());
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
        case ShotVisibleRole:
            return true;
        default:
            break;
        }
    } else {
        // qDebug() << "Shot count:" << shotIndex << chunk->shotCount() << role << DataBoxVisibleRole;

        if(role == ShotVisibleRole) {
            return false;
        }
    }

    return QVariant();
}

/**
 * @brief cwSurveyEditorModel::rowCount
 * @param parent
 * @return
 */
int cwSurveyEditorModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);

    if(Trip.isNull()) {
        return 0;
    }

    if(!IndexToChunk.isEmpty()) {
        auto lastIter = std::prev(IndexToChunk.end());
        return lastIter.key().high();
    }
    return 0;
}

/**
 * @brief cwSurveyEditorModel::roleNames
 * @return
 */
QHash<int, QByteArray> cwSurveyEditorModel::roleNames() const
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
    roles.insert(ChunkRole, "chunk");
    roles.insert(ChunkIdRole, "chunkId");
    roles.insert(StationVisibleRole, "stationVisible");
    roles.insert(ShotVisibleRole, "shotVisible");
    return roles;
}

/**
 * @brief cwSurveyEditorModel::addShotCalibration
 * @param index - The index for the whole trip
 */
void cwSurveyEditorModel::addShotCalibration(int index)
{
    auto chunkIndex = indexToStationChunk(index);
    if(chunkIndex.first != nullptr) {
        chunkIndex.first->addCalibration(chunkIndex.second);

        QModelIndex dataChangeIndex = this->index(index);

        emit dataChanged(dataChangeIndex, dataChangeIndex, QVector<int>() << ShotCalibrationRole);
    }
}

/**
 * @brief cwSurveyEditorModel::updateIndexToChunk
 *
 * This clears the current IndexToChunk and updates it with a new lookup
 */
void cwSurveyEditorModel::updateIndexToChunk()
{
   IndexToChunk.clear();

   if(Trip.isNull()) {
       return;
   }

   int stationCount = 1; //1 enables the tile bar to be displayed for the first chunk in the view
   foreach(cwSurveyChunk* chunk, Trip->chunks()) {
       int begin = stationCount;
       int end = begin + chunk->stationCount() - 1;
       qDebug() << "Insert:" << chunk << begin << end;
       IndexToChunk.insert(Range(begin, end), chunk);
       stationCount += chunk->stationCount() + 1;
   }
}

/**
 * @brief cwSurveyEditorModel::modelIndexToStationChunk
 * @param index - The index from the model
 * @return The station index in the chunk and the chunk at index
 */
QPair<cwSurveyChunk*, int> cwSurveyEditorModel::modelIndexToStationChunk(const QModelIndex& index) const
{
    return indexToStationChunk(index.row());
}

/**
 * @brief cwSurveyEditorModel::indexToStationChunk
 * @param index - The row in model
 * @return The station index in teh chunk and teh cuhnk at index
 */
QPair<cwSurveyChunk*, int> cwSurveyEditorModel::indexToStationChunk(int index) const
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
