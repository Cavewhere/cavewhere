/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWFLOATINGSURVEYMODEL_H
#define CWFLOATINGSURVEYMODEL_H

//Our includes
#include "cwFindFloatingSurveys.h"
#include "cwGlobals.h"
class cwCavingRegion;

//Qt includes
#include <QAbstractListModel>
#include <QList>
#include <QPointer>
#include <QStringList>
#include <QUuid>
#include <QtQml/qqmlregistration.h>

/**
 * The rows behind commit 6's floating-survey banner: one per survey the last
 * run found floating, named the way a person reads them.
 *
 * cwLinePlotManager::floatingSurveys() answers in uuids and in the station
 * spelling the exporter used. A banner needs the cave and trip names the user
 * typed and the station tails the editor shows, so this resolves one against
 * the live region and QML never holds a uuid it would have to look up itself.
 * Which pass found the survey rides along, but no consumer has to branch on it
 * — the two triggers, one banner.
 *
 * The records describe a finished run, never the current edit: a run that
 * stopped early carries records forward, so a row can name a trip the region
 * has since dropped. Those rows simply do not appear.
 */
class CAVEWHERE_LIB_EXPORT cwFloatingSurveyModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(FloatingSurveyModel)
    QML_UNCREATABLE("Owned by LinePlotManager")

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)

public:
    enum Roles {
        CaveNameRole = Qt::UserRole + 1,
        TripNameRole,
        TriggerRole,
        StationsRole,
        CaveIdRole,
        TripIdRole
    };
    Q_ENUM(Roles)

    //! Which pass found the survey, projected from
    //! cwFindFloatingSurveys::Result::Trigger so QML can name the two.
    enum Trigger {
        UnconnectedChunks, //!< pre-solve, native chunks
        ExternalScope      //!< post-solve, an attached centerline's scope
    };
    Q_ENUM(Trigger)

    explicit cwFloatingSurveyModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    //! True when the last run found \a tripId floating, whichever pass found
    //! it. The per-trip surfaces ask this; the whole-region ones bind the rows.
    Q_INVOKABLE bool isFloating(const QUuid& tripId) const;

    //! \a tripId's floating stations, in the trip's own namespace. Empty both
    //! for a trip that is not floating and for one cavern dropped outright, so
    //! it can never stand in for isFloating().
    Q_INVOKABLE QStringList floatingStations(const QUuid& tripId) const;

    //! The region every record's uuids are resolved against. Rows follow it: a
    //! cave or trip that is renamed, added or removed re-renders them.
    void setRegion(cwCavingRegion* region);

    //! The run's answer, exactly as cwLinePlotManager published it.
    void setResults(const QList<cwFindFloatingSurveys::Result>& results);

signals:
    void countChanged();

private:
    struct Row {
        QUuid caveId;
        QUuid tripId;
        QString caveName;
        QString tripName;
        Trigger trigger = ExternalScope;
        QStringList stations;

        bool operator==(const Row& other) const = default;
    };

    QPointer<cwCavingRegion> m_region;
    QList<cwFindFloatingSurveys::Result> m_results;
    QList<Row> m_rows;

    void rebuildRows();
};

#endif // CWFLOATINGSURVEYMODEL_H
