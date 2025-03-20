/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWLEADMODEL_H
#define CWLEADMODEL_H

//Qt includse
#include <QAbstractListModel>
#include <QPointer>
#include <QQmlEngine>

//Our includes
class cwRegionTreeModel;
class cwCave;
#include "cwScrap.h"

/**
 * @brief The cwLeadModel class
 *
 * This models all the leads that are in a cave. Call setCave() for the cave and regionModel
 * to hook up all the scraps.  The cwScrap holds all the leads, and the cave is used to filter
 * the scrap's that are relevant to the cave.
 */
class cwLeadModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LeadModel)

    Q_PROPERTY(cwRegionTreeModel* regionModel READ regionModel WRITE setRegionTreeModel NOTIFY regionModelChanged)
    Q_PROPERTY(cwCave* cave READ cave WRITE setCave NOTIFY caveChanged)
    Q_PROPERTY(QString referanceStation READ referanceStation WRITE setReferanceStation NOTIFY referanceStationChanged)

public:
    enum Roles {
        LeadPositionOnNote = cwScrap::LeadPositionOnNote,
        LeadPosition = cwScrap::LeadPosition,
        LeadDesciption = cwScrap::LeadDesciption,
        LeadSize = cwScrap::LeadSize,
        LeadUnits = cwScrap::LeadUnits,
        LeadSupportedUnits = cwScrap::LeadSupportedUnits,
        LeadCompleted = cwScrap::LeadCompleted,
        LeadSizeAsString = cwScrap::LeadNumberOfRoles + 1,
        LeadNearestStation,
        LeadScrap,
        LeadIndexInScrap,
        LeadDistanceToReferanceStation,
        LeadTrip
    };
    Q_ENUM(Roles)

    explicit cwLeadModel(QObject *parent = 0);
    ~cwLeadModel();

    cwRegionTreeModel* regionModel() const;
    void setRegionTreeModel(cwRegionTreeModel* regionModel);

    cwCave* cave() const;
    void setCave(cwCave* cave);

    QString referanceStation() const;
    void setReferanceStation(QString referanceStation);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    Q_INVOKABLE QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const;
    Q_INVOKABLE QVariant data(const QModelIndex &index, int role) const;
    Q_INVOKABLE bool setData(const QModelIndex &index, const QVariant &value, int role);
    QHash<int, QByteArray> roleNames() const;

signals:
    void regionModelChanged();
    void caveChanged();
    void referanceStationChanged();

public slots:

private:
    QPointer<cwRegionTreeModel> RegionTreeModel; //!<
    QPointer<cwCave> Cave;

    QMap<cwScrap*, int> ScrapToOffset;
    QMap<int, cwScrap*> OffsetToScrap;

    QString ReferanceStation; //!< For calculating the distance to the leads

    void fullModelReset();

    void removeScrap(cwScrap* scrap);
    void addScrap(cwScrap* scrap);

    void updateOffsets(cwScrap* startScrap);
    QString nearestStation(cwScrap* scrap, int leadIndex) const;

    QPair<cwScrap*, int> scrapAndIndex(QModelIndex index) const;

    double leadDistance(cwScrap* scrap, int leadIndex) const;

    void addScrapToOffsetDatabase(cwScrap* scrap);
    void removeScrapFromOffsetDatabase(cwScrap* scrap);

private slots:
    void beginInsertLeads(int begin, int end);
    void endInsertLeads(int begin, int end);
    void beginRemoveLeads(int begin, int end);
    void endRemoveLeads(int begin, int end);
    void leadDataUpdated(int begin, int end, QList<int> roles);
    void scrapDeleted(QObject* scrapObj);
    void insertScraps(QModelIndex parent, int begin, int end);
    void removeScraps(QModelIndex parent, int begin, int end);


};

/**
* @brief cwLeadModel::referanceStation
* @return The station that
*/
inline QString cwLeadModel::referanceStation() const {
    return ReferanceStation;
}

#endif // CWLEADMODEL_H
