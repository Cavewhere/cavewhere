/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRECENTERCANDIDATEMODEL_H
#define CWRECENTERCANDIDATEMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QList>
#include <QQmlEngine>
#include <QString>
#include <QUuid>

//Std includes
#include <optional>

//Our includes
#include "cwGeoPoint.h"
#include "cwGlobals.h"

class cwCavingRegion;
class cwLocalProjectionManager;

//! One station the project's projection could be recentered on: which station it
//! is, how to name it to the user, where on Earth it sits, and whether centering
//! there would keep the project's data inside the frame's reach.
struct cwRecenterCandidate {
    QUuid stationId;
    QString stationName;
    QString caveName;
    //! Where the station is in WGS84, x-first: longitude in x, latitude in y.
    //! Empty when PROJ can't relate the station's own system to WGS84.
    std::optional<cwGeoPoint> position;
    bool eligible = false;
    //! Whether the frame is already derived from this station. Answered by
    //! identity against cwGeoReference::anchor(), never by matching the text the
    //! Centered-on row happens to print.
    bool current = false;
};

/**
 * The fix stations the user may recenter the local projection on, as rows.
 *
 * Owns the question "which stations could the project be centered on, and which
 * of those would be a sane place to center it" — the picker reads it as a model,
 * and cwLocalProjectionManager::recenterOnStation() enforces the same rule
 * through cwLocalProjectionManager::isWithinReach() before it moves anything.
 *
 * Created empty by the manager on first access and filled only by refresh(),
 * which the picker calls as it opens. It is deliberately not kept current
 * between opens, and deliberately does no work to exist: eligibility reprojects
 * every station in the project, and nothing is showing the rows.
 */
class CAVEWHERE_LIB_EXPORT cwRecenterCandidateModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RecenterCandidateModel)
    QML_UNCREATABLE("Owned by LocalProjectionManager; access via region.localProjection.recenterCandidates")

    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)

    //! The middle of the project, the other thing the picker offers to center
    //! on. It rides here rather than on the manager so that one refresh()
    //! computes everything the picker shows, from one look at the project.
    Q_PROPERTY(bool hasDataCenter READ hasDataCenter NOTIFY dataCenterChanged FINAL)
    Q_PROPERTY(double dataCenterLatitude READ dataCenterLatitude NOTIFY dataCenterChanged FINAL)
    Q_PROPERTY(double dataCenterLongitude READ dataCenterLongitude NOTIFY dataCenterChanged FINAL)
    //! Whether the projection is centered on that middle now — the data center's
    //! answer to CurrentRole, so the row can say so and stop offering a move
    //! that would re-derive the frame the project already has.
    Q_PROPERTY(bool dataCenterIsCurrent READ dataCenterIsCurrent NOTIFY dataCenterChanged FINAL)

public:
    enum Roles {
        StationIdRole = Qt::UserRole + 1,
        StationNameRole,
        CaveNameRole,
        //! Where the station is in WGS84 degrees, so the reader can tell two
        //! stations of the same name apart and can catch a fix typed into the
        //! wrong zone before centering the whole project on it.
        LatitudeRole,
        LongitudeRole,
        //! Whether the two above mean anything. A station PROJ can't place has
        //! no coordinate to show, and a zero would read as one off West Africa.
        HasCoordinateRole,
        //! Whether recentering on this station is allowed. An ineligible row
        //! stays in the list rather than being dropped: a station the project's
        //! own data sits 200 km from is exactly the one a user goes looking for,
        //! and a row that is present but disabled answers them.
        EligibleRole,
        //! Whether the projection is centered on this station now, so the row
        //! can say so and stop offering a move that would change nothing.
        CurrentRole
    };
    Q_ENUM(Roles)

    cwRecenterCandidateModel(cwLocalProjectionManager* manager, cwCavingRegion* region);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_candidates.size()); }

    bool hasDataCenter() const { return m_dataCenter.has_value(); }
    double dataCenterLatitude() const { return m_dataCenter.has_value() ? m_dataCenter->y : 0.0; }
    double dataCenterLongitude() const { return m_dataCenter.has_value() ? m_dataCenter->x : 0.0; }
    bool dataCenterIsCurrent() const { return m_dataCenterIsCurrent; }

    //! Rebuild the rows against the project as it stands. Called as the picker
    //! opens, and cheap enough to be: it reprojects the project's inputs.
    Q_INVOKABLE void refresh();

signals:
    void countChanged();
    void dataCenterChanged();

private:
    cwLocalProjectionManager* m_manager = nullptr;
    cwCavingRegion* m_region = nullptr;

    QList<cwRecenterCandidate> m_candidates;

    //! The middle of the project in WGS84, x-first, as of the last refresh().
    std::optional<cwGeoPoint> m_dataCenter;
    bool m_dataCenterIsCurrent = false;

    void setCandidates(const QList<cwRecenterCandidate>& candidates);
    void setDataCenter(const std::optional<cwGeoPoint>& center, bool isCurrent);

    //! \a point, read in \a coordinateSystem, as WGS84 degrees x-first. Empty
    //! when PROJ can't relate the two, which is the only honest answer for a
    //! coordinate the picker would otherwise print as a place on Earth.
    static std::optional<cwGeoPoint> toWgs84(const QString& coordinateSystem,
                                             const cwGeoPoint& point);
};

#endif // CWRECENTERCANDIDATEMODEL_H
