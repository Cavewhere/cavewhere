/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTIESUGGESTIONMODEL_H
#define CWTIESUGGESTIONMODEL_H

//Our includes
#include "cwGlobals.h"
#include "cwStationHandle.h"
class cwCave;
class cwCavingRegion;
class cwTrip;

//Qt includes
#include <QAbstractListModel>
#include <QList>
#include <QPointer>
#include <QString>
#include <QtQml/qqmlregistration.h>

/**
 * The ties a floating trip could plausibly make, one per row, ready to create.
 *
 * Set trip and the model lists pairs of stations that look like they name the
 * same physical point: one station of that trip, one station of another trip in
 * the same cave. tieAt() creates the pair the row names, which is the whole
 * point of the type — a view shows a row and calls tieAt() on it, and never
 * assembles a tie itself.
 *
 * A trip floats when nothing joins its survey to the rest of its cave, so it has
 * no position in the cave's frame and cannot be ranked by distance to anything.
 * Rows are therefore ranked by name alone: stations that share a name first
 * (Match::SameName), then stations whose names start with the same letters
 * (Match::SameLetterPrefix). That is not a first cut of distance ranking — it is
 * the complete answer for this surface, because the tie a row creates ends the
 * float, and a trip that no longer floats has coordinates and needs a different
 * surface to refine.
 *
 * Names are compared by their last dotted component, never whole. An attached
 * centerline keeps its file's own "*begin" nesting inside the trip's scope, so
 * its station reads "simple.a1" where the native station beside it reads "a1";
 * comparing whole would find nothing for exactly the surveys that float.
 *
 * Candidates come from the trip's own cave only. Tying to another cave is a
 * different gesture with a different home (cwCavingRegion::equates), and it is
 * never what stops a survey floating in the cave it belongs to.
 */
class CAVEWHERE_LIB_EXPORT cwTieSuggestionModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TieSuggestionModel)

    Q_PROPERTY(cwTrip* trip READ trip WRITE setTrip NOTIFY tripChanged FINAL)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)
    Q_PROPERTY(bool truncated READ truncated NOTIFY countChanged FINAL)

public:
    //! Why a row is being suggested — the ranking, in order.
    enum Match {
        SameName,        //!< the two stations are named alike
        SameLetterPrefix //!< their names start with the same letters
    };
    Q_ENUM(Match)

    enum Roles {
        StationRole = Qt::UserRole + 1, //!< the floating trip's station, its tail
        CandidateStationRole,           //!< the station it would tie to, its tail
        CandidateTripNameRole,          //!< the trip that station belongs to
        MatchRole                       //!< Match, why this pair is suggested
    };
    Q_ENUM(Roles)

    explicit cwTieSuggestionModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    cwTrip* trip() const { return m_trip; }
    void setTrip(cwTrip* trip);

    //! True when there were more suggestions than a person should be handed at
    //! once and the rest were left out. A view that shows every row says so.
    bool truncated() const { return m_truncated; }

    //! Record the tie \a row names, in the home cwCavingRegion::tieStations
    //! picks. Returns what that verb returns, so false means the pair cannot be
    //! tied rather than that nothing happened to change.
    Q_INVOKABLE bool tieAt(int row);

signals:
    void tripChanged();
    void countChanged();

private:
    struct Row {
        cwStationHandle station;
        cwStationHandle candidate;
        QString candidateTripName;
        Match match = SameName;
    };

    QPointer<cwTrip> m_trip;
    //The cave and region the model connected to, remembered so disconnecting
    //still finds them after the trip is destroyed or leaves its cave.
    QPointer<cwCave> m_connectedCave;
    QPointer<cwCavingRegion> m_connectedRegion;
    QList<Row> m_rows;
    bool m_truncated = false;

    void setTripConnected(cwTrip* trip, bool connected);
    void rebuildRows();
};

#endif // CWTIESUGGESTIONMODEL_H
