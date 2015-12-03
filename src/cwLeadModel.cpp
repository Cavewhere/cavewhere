/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLeadModel.h"
#include "cwRegionTreeModel.h"
#include "cwCave.h"
#include "cwScrap.h"
#include "cwTrip.h"
#include "cwSurveyNoteModel.h"
#include "cwNote.h"
#include "cwSurveyChunk.h"

//Std includes
#include <limits>

cwLeadModel::cwLeadModel(QObject *parent) : QAbstractListModel(parent)
{

}

cwLeadModel::~cwLeadModel()
{

}

/**
 * @brief cwLeadModel::rowCount
 * @param parent
 * @return The number of leads in the model
 */
int cwLeadModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    if(OffsetToScrap.isEmpty()) { return 0; }
    int offset = OffsetToScrap.lastKey();
    cwScrap* scrap = OffsetToScrap.last();
    return offset + scrap->numberOfLeads();
}

/**
 * @brief cwLeadModel::index
 * @param row
 * @param column
 * @param parent
 * @return
 *
 * The same as QAbstractListModel, just invokable.
 */
QModelIndex cwLeadModel::index(int row, int column, const QModelIndex &parent) const
{
    return QAbstractListModel::index(row, column, parent);
}

/**
 * @brief cwLeadModel::data
 * @param index
 * @param role
 * @return The data of the lead at index
 */
QVariant cwLeadModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) { return QVariant(); }

    auto scrapIndex = scrapAndIndex(index);
    cwScrap* scrap = scrapIndex.first;
    int leadIndex = scrapIndex.second;

    Q_ASSERT(leadIndex >= 0);
    Q_ASSERT(leadIndex < scrap->numberOfLeads());

    if(role < (int)cwScrap::LeadNumberOfRoles) {
        return scrap->leadData((cwScrap::LeadDataRole)role, leadIndex);
    }

    switch(role) {
    case LeadSizeAsString: {
        const cwLead& lead = scrap->leads().at(leadIndex);
        //Check for nan
        QString width = lead.size().width() == lead.size().width() ? QString("%1").arg(lead.size().width()) : "?";
        QString height = lead.size().height() == lead.size().height() ? QString("%1").arg(lead.size().height()) : "?";
        return QString("%1 x %2")
                .arg(width)
                .arg(height);
        }
    case LeadNearestStation:
        return nearestStation(scrap, leadIndex);
    case LeadScrap:
        return QVariant::fromValue(scrap);
    case LeadIndexInScrap:
        return leadIndex;
    case LeadDistanceToReferanceStation:
        return leadDistance(scrap, leadIndex);
    default:
        return QVariant();
    }
    return QVariant();
}

/**
 * @brief cwLeadModel::setData
 * @param index
 * @param value
 * @param role
 * @return Returns true if success and false if the data wasn't set
 *
 * Currently this only supports LeadCompleted, all other roles will be ignored and false
 * will be returned
 */
bool cwLeadModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if(!index.isValid()) { return false; }

    auto scrapIndex = scrapAndIndex(index);
    cwScrap* scrap = scrapIndex.first;
    int leadIndex = scrapIndex.second;

    Q_ASSERT(leadIndex >= 0);
    Q_ASSERT(leadIndex < scrap->numberOfLeads());

    switch(role) {
    case LeadCompleted:
        scrap->setLeadData(cwScrap::LeadCompleted, leadIndex, value);
        return true;
    default:
        return false;
    }
    return false;
}

/**
 * @brief cwLeadModel::roleNames
 * @return The role names for the leadModel
 */
QHash<int, QByteArray> cwLeadModel::roleNames() const
{
    QHash<int, QByteArray> names;
    names.insert(LeadPositionOnNote, "leadPositionOnNote");
    names.insert(LeadPosition, "leadPosition");
    names.insert(LeadDesciption, "leadDescription");
    names.insert(LeadSize, "leadSize");
    names.insert(LeadUnits, "leadUnits");
    names.insert(LeadSupportedUnits, "leadSupportedUnits");
    names.insert(LeadCompleted, "leadCompleted");
    names.insert(LeadSizeAsString, "leadSizeAsString");
    names.insert(LeadNearestStation, "leadNearestStation");
    names.insert(LeadDistanceToReferanceStation, "leadDistanceToReferanceStation");
    return names;
}

/**
 * @brief cwLeadModel::fullModelReset
 *
 * This goes through all the scraps in the leadModel, cave
 */
void cwLeadModel::fullModelReset()
{
   if(regionModel() == nullptr || cave() == nullptr) { return; }

   beginResetModel();

   //Remove all the scraps
   foreach(cwScrap* scrap, ScrapToOffset.keys()) {
       removeScrap(scrap);
   }

   //Add all the scraps
   foreach(cwTrip* trip, cave()->trips()) {
       foreach(cwNote* note, trip->notes()->notes()) {
           foreach(cwScrap* scrap, note->scraps()) {
               addScrap(scrap);
           }
       }
   }

   if(cave()->tripCount() > 0
           && !cave()->trip(0)->chunks().isEmpty()
           && cave()->trip(0)->chunks().first()->stationCount() > 0)
   {
        QString firstStation = cave()->trip(0)->chunks().first()->station(0).name();
        setReferanceStation(firstStation);
   }

   endResetModel();

}

/**
 * @brief cwLeadModel::removeScrap
 * @param scrap
 *
 * This is a helper function that will remove a scrap to the leadModel. This will disconnect the
 * scrap from the model, and remove it from the offset database.
 */
void cwLeadModel::removeScrap(cwScrap *scrap)
{
    removeScrapFromOffsetDatabase(scrap);
    disconnect(scrap, 0, this, 0);
}

/**
 * @brief cwLeadModel::addScrap
 * @param scrap
 *
 * This is helper function that will add scrap to the leadModel. This will connect the scrap
 * to the model and update offset database.
 */
void cwLeadModel::addScrap(cwScrap *scrap)
{
    Q_ASSERT(!ScrapToOffset.contains(scrap));

    if(scrap->numberOfLeads() > 0) {
        addScrapToOffsetDatabase(scrap);
    }

    connect(scrap, &cwScrap::leadsBeginInserted, this, &cwLeadModel::beginInsertLeads);
    connect(scrap, &cwScrap::leadsInserted, this, &cwLeadModel::endInsertLeads);
    connect(scrap, &cwScrap::leadsRemoved, this, &cwLeadModel::beginRemoveLeads);
    connect(scrap, &cwScrap::leadsRemoved, this, &cwLeadModel::endRemoveLeads);
    connect(scrap, &cwScrap::leadsDataChanged, this, &cwLeadModel::leadDataUpdated);
    connect(scrap, SIGNAL(destroyed(QObject*)), this, SLOT(scrapDeleted(QObject*)));
}

/**
 * @brief cwLeadModel::updateOffsets
 * @param startScrap
 *
 * This updates all the offset's after startScrap
 */
void cwLeadModel::updateOffsets(cwScrap *startScrap)
{
    int offset = ScrapToOffset.value(startScrap);
    int nextOffset = offset + startScrap->numberOfLeads();

    auto iter = OffsetToScrap.lowerBound(offset);
    Q_ASSERT(iter != OffsetToScrap.end());

    iter++;

    bool updateOffset = false;
    for(; iter != OffsetToScrap.end(); iter++) {
        cwScrap* currentScrap = iter.value();
        ScrapToOffset.insert(currentScrap, nextOffset);
        nextOffset = nextOffset + currentScrap->numberOfLeads();
        updateOffset = true;
    }

    if(updateOffset) {
        QMap<int, cwScrap*> offsetToScrap;
        for(auto iter = ScrapToOffset.begin(); iter != ScrapToOffset.end(); iter++) {
            offsetToScrap.insert(iter.value(), iter.key());
        }
        OffsetToScrap = offsetToScrap;
    }

}

/**
 * @brief cwLeadModel::beginInsertLeads
 * @param begin
 * @param end
 *
 * Called when a scrap adds leads
 */
void cwLeadModel::beginInsertLeads(int begin, int end)
{
    Q_ASSERT(qobject_cast<cwScrap*>(sender())  != nullptr);
    cwScrap* scrap = static_cast<cwScrap*>(sender());

    addScrapToOffsetDatabase(scrap);

    Q_ASSERT(ScrapToOffset.contains(scrap));

    int offset = ScrapToOffset.value(scrap);
    int offsetBegin = offset + begin;
    int offsetEnd = offset + end;

    beginInsertRows(QModelIndex(), offsetBegin, offsetEnd);
}

/**
 * @brief cwLeadModel::insertLeads
 * @param begin
 * @param end
 */
void cwLeadModel::endInsertLeads(int begin, int end)
{
    Q_UNUSED(begin);
    Q_UNUSED(end);

    Q_ASSERT(qobject_cast<cwScrap*>(sender())  != nullptr);
    cwScrap* scrap = static_cast<cwScrap*>(sender());

    updateOffsets(scrap);

    endInsertRows();
}

/**
 * @brief cwLeadModel::beginRemoveLeads
 * @param begin
 * @param end
 */
void cwLeadModel::beginRemoveLeads(int begin, int end)
{
    Q_ASSERT(qobject_cast<cwScrap*>(sender())  != nullptr);
    cwScrap* scrap = static_cast<cwScrap*>(sender());
    Q_ASSERT(ScrapToOffset.contains(scrap));

    int offset = ScrapToOffset.value(scrap);
    int offsetBegin = offset + begin;
    int offsetEnd = offset + end;

    beginRemoveRows(QModelIndex(), offsetBegin, offsetEnd);
}

/**
 * @brief cwLeadModel::endRemoveLeads
 * @param begin
 * @param end
 */
void cwLeadModel::endRemoveLeads(int begin, int end)
{
    Q_UNUSED(begin);
    Q_UNUSED(end);

    Q_ASSERT(qobject_cast<cwScrap*>(sender())  != nullptr);
    cwScrap* scrap = static_cast<cwScrap*>(sender());

    if(scrap->numberOfLeads() == 0) {
        removeScrapFromOffsetDatabase(scrap);
    }

    updateOffsets(scrap);

    endInsertRows();
}

/**
 * @brief cwLeadModel::leadDataUpdated
 * @param begin
 * @param end
 * @param roles
 */
void cwLeadModel::leadDataUpdated(int begin, int end, QList<int> roles)
{
    Q_ASSERT(qobject_cast<cwScrap*>(sender())  != nullptr);
    cwScrap* scrap = static_cast<cwScrap*>(sender());
    Q_ASSERT(ScrapToOffset.contains(scrap));

    int offset = ScrapToOffset.value(scrap);
    int offsetBegin = offset + begin;
    int offsetEnd = offset + end;

    dataChanged(index(offsetBegin), index(offsetEnd), roles.toVector());
}

/**
 * @brief cwLeadModel::scrapDeleted
 * @param scrapObj
 *
 * Called when a scrap is deleted
 */
void cwLeadModel::scrapDeleted(QObject *scrapObj)
{
    Q_ASSERT(qobject_cast<cwScrap*>(scrapObj) != nullptr);
    cwScrap* scrap = static_cast<cwScrap*>(scrapObj);

    removeScrap(scrap);
}

/**
 * @brief cwLeadModel::scrapInserted
 * @param parent
 * @param begin
 * @param end
 *
 * This is called when the region tree model inserts scraps. This will only add scraps that
 * are in the current cave.
 */
void cwLeadModel::insertScraps(QModelIndex parent, int begin, int end)
{
    if(RegionTreeModel->isNote(parent)) {
        cwNote* note = RegionTreeModel->note(parent);
        if(note->parentCave() == cave()) {
            for(int i = begin; i <= end; i++) {
                cwScrap* scrap = note->scrap(i);
                addScrap(scrap);
            }
        }
    }
}

/**
 * @brief cwLeadModel::scrapRemoved
 * @param parent
 * @param begin
 * @param end
 *
 * This is called when the region tree model removes scraps. This will only remove scraps that
 * are in the current cave.
 */
void cwLeadModel::removeScraps(QModelIndex parent, int begin, int end)
{
    if(RegionTreeModel->isNote(parent)) {
        cwNote* note = RegionTreeModel->note(parent);
        if(note->parentCave() == cave()) {
            for(int i = begin; i <= end; i++) {
                cwScrap* scrap = note->scrap(i);
                removeScrap(scrap);
            }
        }
    }
}

/**
 * @brief cwLeadModel::nearestStation
 * @param scrap
 * @param leadIndex
 * @return Returns the nearest station to the lead
 *
 * This will go through the stations in the scrap, and find the nearest survey station
 */
QString cwLeadModel::nearestStation(cwScrap *scrap, int leadIndex) const
{
    QString nearestStation;
    double nearestDistance = std::numeric_limits<double>::max();
    const cwLead& lead = scrap->leads().at(leadIndex);
    foreach(const cwNoteStation& noteStation, scrap->stations()) {
        QLineF line(noteStation.positionOnNote(), lead.positionOnNote());
        double length = line.length();
        if(length < nearestDistance) {
            nearestDistance = length;
            nearestStation = noteStation.name();
        }
    }

    return nearestStation;
}

/**
 * @brief cwLeadModel::scrapAndIndex
 * @param index - The index that the scrap and local index will be converted to
 * @return A pair, made up of the scrap (first) and local index of the lead (second)
 */
QPair<cwScrap *, int> cwLeadModel::scrapAndIndex(QModelIndex index) const
{
    auto iter = OffsetToScrap.lowerBound(index.row());
    if(iter.key() > index.row()) {
        iter--;
    }

    Q_ASSERT(iter.key() <= index.row());

    cwScrap* scrap = iter.value();
    int leadIndex = index.row() - iter.key();

    return QPair<cwScrap*, int>(scrap, leadIndex);
}

/**
 * @brief cwLeadModel::leadDistance
 * @param scrap
 * @param leadIndex
 * @return The distance from lead in scrap to the referance station, in meters.
 */
double cwLeadModel::leadDistance(cwScrap *scrap, int leadIndex) const
{
    if(cave()->stationPositionLookup().hasPosition(referanceStation())) {
        QVector3D stationPosition = cave()->stationPositionLookup().position(referanceStation());
        QVector3D leadPosition = scrap->leadData(cwScrap::LeadPosition, leadIndex).value<QVector3D>();
        QVector3D diff = stationPosition - leadPosition;
        return diff.length();
    }
    return 0.0;
}

/**
 * @brief cwLeadModel::addScrapToOffsetDatabase
 * @param scrap
 *
 * This tries to add the scrap to the offset database
 *
 * If the scrap doesn't have any leads, this does nothing
 */
void cwLeadModel::addScrapToOffsetDatabase(cwScrap *scrap)
{
    if(!ScrapToOffset.contains(scrap)) {
        int lastOffset = 0;
        int lastNumberOfLeads = 0;

        if(!OffsetToScrap.isEmpty()) {
            lastOffset = OffsetToScrap.lastKey();
            lastNumberOfLeads = OffsetToScrap.last()->numberOfLeads();
        }

        int offset = lastOffset + lastNumberOfLeads;

        OffsetToScrap.insert(offset, scrap);
        ScrapToOffset.insert(scrap, offset);
    }
}

/**
 * @brief cwLeadModel::removeScrapFromOffsetDatabase
 * @param scrap
 *
 * This tries to remove the scrap from the offset database
 */
void cwLeadModel::removeScrapFromOffsetDatabase(cwScrap *scrap)
{
    if(ScrapToOffset.contains(scrap)) {
        int offset = ScrapToOffset.value(scrap);
        Q_ASSERT(OffsetToScrap.contains(offset));

        ScrapToOffset.remove(scrap);
        OffsetToScrap.remove(offset);
    }
}

/**
* @brief cwLeadModel::regionModel
* @return
*/
cwRegionTreeModel* cwLeadModel::regionModel() const {
    return RegionTreeModel;
}

/**
* @brief cwLeadModel::setRegionTreeModel
* @param regionModel
*/
void cwLeadModel::setRegionTreeModel(cwRegionTreeModel* regionModel) {
    if(RegionTreeModel != regionModel) {
        if(!RegionTreeModel.isNull()) {
            disconnect(RegionTreeModel.data(), 0, this, 0);
        }

        RegionTreeModel = regionModel;

        if(!RegionTreeModel.isNull()) {
            connect(RegionTreeModel.data(), &cwRegionTreeModel::rowsInserted, this, &cwLeadModel::insertScraps);
            connect(RegionTreeModel.data(), &cwRegionTreeModel::rowsAboutToBeRemoved, this, &cwLeadModel::removeScraps);
        }

        fullModelReset();
        emit regionModelChanged();
    }
}

/**
* @brief cwLeadModel::cave
* @return
*/
cwCave* cwLeadModel::cave() const {
    return Cave;
}

/**
* @brief cwLeadModel::setCave
* @param cave
*/
void cwLeadModel::setCave(cwCave* cave) {
    if(Cave != cave) {
        Cave = cave;
        fullModelReset();
        emit caveChanged();
    }
}

/**
* @brief cwLeadModel::setReferanceStation
* @param referanceStation
*
* This is used to calculate distance to the lead from the referanceStation. The distance
* is only line of sight.
*/
void cwLeadModel::setReferanceStation(QString referanceStation) {
    if(ReferanceStation != referanceStation) {
        ReferanceStation = referanceStation;

        if(rowCount() > 0) {
            QModelIndex first = index(0);
            QModelIndex last = index(rowCount() - 1);

            QVector<int> roles;
            roles.append(LeadDistanceToReferanceStation);

            emit dataChanged(first, last, roles);
        }

        emit referanceStationChanged();
    }
}
