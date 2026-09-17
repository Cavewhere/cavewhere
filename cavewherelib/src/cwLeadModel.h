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
#include "cwRegionTreeModel.h"
// class cwRegionTreeModel;
class cwCave;
#include "cwScrap.h"

/**
 * @brief The cwLeadModel class
 *
 * This models all the leads that are in a cave. Call setCave() for the cave and regionModel
 * to hook up all the scraps.  The cwScrap holds all the leads, and the cave is used to filter
 * the scrap's that are relevant to the cave.
 */
class CAVEWHERE_LIB_EXPORT cwLeadModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LeadModel)

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
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
    void countChanged();
    void regionModelChanged();
    void caveChanged();
    void referanceStationChanged();

public slots:

private:
    QPointer<cwRegionTreeModel> m_regionTreeModel; //!<
    QPointer<cwCave> m_cave;

    //! Every scrap connected to the model, in row order. Each contributes as many
    //! rows as it holds leads, so a scrap without leads is still a member.
    QList<cwScrap*> m_scraps;

    //! Where each scrap's leads start, followed by the total row count. Rebuilt from
    //! m_scraps on demand, and empty for as long as it needs rebuilding, so m_scraps
    //! stays the one place the row order is recorded.
    mutable QList<int> m_firstRows;

    QString m_referanceStation; //!< For calculating the distance to the leads

    void fullModelReset();

    void removeScrap(cwScrap* scrap);
    void addScrap(cwScrap* scrap);

    void detachScrap(cwScrap* scrap);
    void attachScrap(cwScrap* scrap);

    void invalidateRows();
    void updateRows() const;
    int firstRowOf(cwScrap* scrap) const;

    QString nearestStation(cwScrap* scrap, int leadIndex) const;

    QPair<cwScrap*, int> scrapAndIndex(QModelIndex index) const;

    double leadDistance(cwScrap* scrap, int leadIndex) const;

private slots:
    void beginInsertLeads(int begin, int end);
    void endInsertLeads();
    void beginRemoveLeads(int begin, int end);
    void endRemoveLeads();
    void leadDataUpdated(cwScrap *scrap, int begin, int end, const QList<int>& roles);
    void scrapDeleted(QObject* scrapObj);
    void insertScraps(QModelIndex parent, int begin, int end);
    void removeScraps(QModelIndex parent, int begin, int end);


};

/**
* @brief cwLeadModel::referanceStation
* @return The station that
*/
inline QString cwLeadModel::referanceStation() const {
    return m_referanceStation;
}

#endif // CWLEADMODEL_H
