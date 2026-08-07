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
#include "cwTrip.h"

//Std includes
#include <algorithm>
#include <limits>
#include <utility>

cwLeadModel::cwLeadModel(QObject *parent) : QAbstractListModel(parent)
{
    //Derived from the model's own change signals, so every lead added, lead
    //removed and cave switch announces the count from one place.
    connect(this, &QAbstractItemModel::rowsInserted, this, &cwLeadModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &cwLeadModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &cwLeadModel::countChanged);
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
    updateRows();
    return m_firstRows.constLast();
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

    const auto [scrap, leadIndex] = scrapAndIndex(index);

    if(scrap == nullptr) { return QVariant(); }

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
    case LeadTrip:
        return scrap->parentNote()->parentTrip()->name();
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

    const auto [scrap, leadIndex] = scrapAndIndex(index);

    if(scrap == nullptr) { return false; }

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
    names.insert(LeadTrip, "leadTrip");
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
   for(cwScrap* scrap : std::as_const(m_scraps)) {
       disconnect(scrap, nullptr, this, nullptr);
   }
   m_scraps.clear();
   invalidateRows();

   //Add all the scraps
   foreach(cwTrip* trip, cave()->trips()) {
       foreach(cwNote* note, trip->notes()->notes()) {
           foreach(cwScrap* scrap, note->scraps()) {
               attachScrap(scrap);
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
 * Removes scrap from the leadModel, announcing the rows that its leads occupied.
 */
void cwLeadModel::removeScrap(cwScrap *scrap)
{
    const int firstRow = firstRowOf(scrap);
    const int leadCount = scrap->numberOfLeads();

    if(firstRow < 0 || leadCount == 0) {
        detachScrap(scrap);
        return;
    }

    beginRemoveRows(QModelIndex(), firstRow, firstRow + leadCount - 1);
    detachScrap(scrap);
    endRemoveRows();
}

/**
 * @brief cwLeadModel::addScrap
 * @param scrap
 *
 * Adds scrap to the leadModel, announcing the rows for the leads it arrives with.
 * Loading a project and undoing a scrap removal both hand over a scrap that
 * already holds leads.
 */
void cwLeadModel::addScrap(cwScrap *scrap)
{
    //fullModelReset() walks the cave and insertScraps() listens to the region
    //tree, so the same scrap can arrive twice. The one the model holds stays put.
    if(m_scraps.contains(scrap)) { return; }

    const int leadCount = scrap->numberOfLeads();

    if(leadCount == 0) {
        attachScrap(scrap);
        return;
    }

    const int firstRow = rowCount();

    beginInsertRows(QModelIndex(), firstRow, firstRow + leadCount - 1);
    attachScrap(scrap);
    endInsertRows();
}

/**
 * @brief cwLeadModel::detachScrap
 * @param scrap
 *
 * Drops scrap from the row order and disconnects it. Taking it out of m_scraps
 * slides the scraps after it up by however many leads it held.
 *
 * This only uses scrap as a lookup key, which is all scrapDeleted() allows.
 */
void cwLeadModel::detachScrap(cwScrap *scrap)
{
    m_scraps.removeOne(scrap);
    invalidateRows();
    disconnect(scrap, nullptr, this, nullptr);
}

/**
 * @brief cwLeadModel::attachScrap
 * @param scrap
 *
 * Puts scrap at the end of the row order and connects it to the model.
 *
 * attachScrap() and detachScrap() say nothing about rows, so callers that
 * change the row count go through addScrap() and removeScrap() instead.
 */
void cwLeadModel::attachScrap(cwScrap *scrap)
{
    Q_ASSERT(!m_scraps.contains(scrap));

    m_scraps.append(scrap);
    invalidateRows();

    connect(scrap, &cwScrap::leadsBeginInserted, this, &cwLeadModel::beginInsertLeads);
    connect(scrap, &cwScrap::leadsInserted, this, &cwLeadModel::endInsertLeads);
    connect(scrap, &cwScrap::leadsBeginRemoved, this, &cwLeadModel::beginRemoveLeads);
    connect(scrap, &cwScrap::leadsRemoved, this, &cwLeadModel::endRemoveLeads);
    connect(scrap, &cwScrap::leadsReset, this, &cwLeadModel::fullModelReset);
    connect(scrap, &cwScrap::leadsDataChanged, this, [this, scrap](int begin, int end, const QList<int>& roles) {
        leadDataUpdated(scrap, begin, end, roles);
    });
    connect(scrap, &cwScrap::destroyed, this, &cwLeadModel::scrapDeleted);
}

/**
 * @brief cwLeadModel::invalidateRows
 *
 * Called whenever the scraps, or the leads in them, stop matching m_firstRows.
 */
void cwLeadModel::invalidateRows()
{
    m_firstRows.clear();
}

/**
 * @brief cwLeadModel::updateRows
 *
 * Makes each scrap's first row the running sum of the lead counts before it, then
 * appends the total. A rebuilt m_firstRows always holds that total, so an empty one
 * means it needs rebuilding.
 */
void cwLeadModel::updateRows() const
{
    if(!m_firstRows.isEmpty()) { return; }

    m_firstRows.reserve(m_scraps.size() + 1);

    int firstRow = 0;
    for(const cwScrap* scrap : m_scraps) {
        m_firstRows.append(firstRow);
        firstRow += scrap->numberOfLeads();
    }

    m_firstRows.append(firstRow);
}

/**
 * @brief cwLeadModel::firstRowOf
 * @param scrap
 * @return The row of scrap's first lead, or -1 if the model isn't holding scrap
 */
int cwLeadModel::firstRowOf(cwScrap *scrap) const
{
    const int scrapIndex = m_scraps.indexOf(scrap);
    if(scrapIndex < 0) { return -1; }

    updateRows();
    return m_firstRows.at(scrapIndex);
}

/**
 * @brief cwLeadModel::beginInsertLeads
 * @param begin
 * @param end
 *
 * Called before a scrap adds leads
 */
void cwLeadModel::beginInsertLeads(int begin, int end)
{
    Q_ASSERT(qobject_cast<cwScrap*>(sender())  != nullptr);
    cwScrap* scrap = static_cast<cwScrap*>(sender());

    const int firstRow = firstRowOf(scrap);
    Q_ASSERT(firstRow >= 0);

    beginInsertRows(QModelIndex(), firstRow + begin, firstRow + end);
}

/**
 * @brief cwLeadModel::endInsertLeads
 *
 * Called once a scrap holds its new leads
 */
void cwLeadModel::endInsertLeads()
{
    invalidateRows();
    endInsertRows();
}

/**
 * @brief cwLeadModel::beginRemoveLeads
 * @param begin
 * @param end
 *
 * Called before a scrap removes leads
 */
void cwLeadModel::beginRemoveLeads(int begin, int end)
{
    Q_ASSERT(qobject_cast<cwScrap*>(sender())  != nullptr);
    cwScrap* scrap = static_cast<cwScrap*>(sender());

    const int firstRow = firstRowOf(scrap);
    Q_ASSERT(firstRow >= 0);

    beginRemoveRows(QModelIndex(), firstRow + begin, firstRow + end);
}

/**
 * @brief cwLeadModel::endRemoveLeads
 *
 * Called once a scrap has let go of its leads
 */
void cwLeadModel::endRemoveLeads()
{
    invalidateRows();
    endRemoveRows();
}

/**
 * @brief cwLeadModel::leadDataUpdated
 * @param begin
 * @param end
 * @param roles
 */
void cwLeadModel::leadDataUpdated(cwScrap* scrap, int begin, int end, const QList<int>& roles)
{
    const int firstRow = firstRowOf(scrap);

    if(firstRow < 0 || begin > end) {
        return;
    }

    emit dataChanged(index(firstRow + begin), index(firstRow + end), roles);
}

/**
 * @brief cwLeadModel::scrapDeleted
 * @param scrapObj
 *
 * Called when a scrap is deleted. The region tree model announces scrap removal
 * for cave, trip, note and scrap deletion alike, so removeScraps() handles those
 * while the scrap is still whole. This detaches quietly for the rest: a scrap
 * dying with its parent, and cwNote::setScraps(), which announces a scrapsReset()
 * that the region tree model leaves alone.
 *
 * Note: this is connected to QObject::destroyed(QObject*), which fires from
 * inside ~QObject() after the cwScrap-derived vtable has been reset. That
 * means qobject_cast<cwScrap*>(scrapObj) returns nullptr here even though
 * the pointer is genuinely a cwScrap* — only static_cast is valid, and only
 * as a lookup key (no member access).
 */
void cwLeadModel::scrapDeleted(QObject *scrapObj)
{
    cwScrap* scrap = static_cast<cwScrap*>(scrapObj);
    detachScrap(scrap);
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
    if(m_regionTreeModel->isNote(parent)) {
        cwNote* note = m_regionTreeModel->note(parent);
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
    if(m_regionTreeModel->isNote(parent)) {
        cwNote* note = m_regionTreeModel->note(parent);
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
 * @return A pair, made up of the scrap (first) and local index of the lead
 * (second), or a null scrap if index is past the last lead
 */
QPair<cwScrap *, int> cwLeadModel::scrapAndIndex(QModelIndex index) const
{
    updateRows();

    if(index.row() < 0 || index.row() >= m_firstRows.constLast()) {
        return QPair<cwScrap*, int>(nullptr, -1);
    }

    //The last scrap that starts at or before the row. Scraps without leads
    //repeat their neighbor's first row, and upper_bound lands past that run,
    //so backing up one always reaches the scrap that owns the row. The row is
    //below the total, so upper_bound stops at the total at the latest.
    const auto iter = std::upper_bound(m_firstRows.constBegin(), m_firstRows.constEnd(), index.row());
    const int scrapIndex = static_cast<int>(iter - m_firstRows.constBegin()) - 1;

    Q_ASSERT(scrapIndex >= 0);

    return QPair<cwScrap*, int>(m_scraps.at(scrapIndex), index.row() - m_firstRows.at(scrapIndex));
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
* @brief cwLeadModel::regionModel
* @return
*/
cwRegionTreeModel* cwLeadModel::regionModel() const {
    return m_regionTreeModel;
}

/**
* @brief cwLeadModel::setRegionTreeModel
* @param regionModel
*/
void cwLeadModel::setRegionTreeModel(cwRegionTreeModel* regionModel) {
    if(m_regionTreeModel != regionModel) {
        if(!m_regionTreeModel.isNull()) {
            disconnect(m_regionTreeModel.data(), 0, this, 0);
        }

        m_regionTreeModel = regionModel;

        if(!m_regionTreeModel.isNull()) {
            connect(m_regionTreeModel.data(), &cwRegionTreeModel::rowsInserted, this, &cwLeadModel::insertScraps);
            connect(m_regionTreeModel.data(), &cwRegionTreeModel::rowsAboutToBeRemoved, this, &cwLeadModel::removeScraps);
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
    return m_cave;
}

/**
* @brief cwLeadModel::setCave
* @param cave
*/
void cwLeadModel::setCave(cwCave* cave) {
    if(m_cave != cave) {
        m_cave = cave;
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
    if(m_referanceStation != referanceStation) {
        m_referanceStation = referanceStation;

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
