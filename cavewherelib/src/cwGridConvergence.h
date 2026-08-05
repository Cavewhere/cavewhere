/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGRIDCONVERGENCE_H
#define CWGRIDCONVERGENCE_H

//Our includes
#include "cwGeoPoint.h"
#include "cwGlobals.h"
#include "cwFixStation.h"

//Monad include
#include "Monad/Result.h"

//Qt includes
#include <QObject>
#include <QString>
#include <QList>
#include <QQmlEngine>

/**
 * Per-cave grid-convergence readout — the angle between true north and grid
 * north for a projected coordinate system at the cave's fix-station location.
 *
 * An instance caches the structured PROJ result (angle, state, station, CS)
 * and is recomputed only when the inputs change via update(); the proj_factors
 * work never runs on a plain property read. The display strings (text /
 * detailText) are deliberately *computed* from that cached state on each read
 * rather than stored, so they can never drift from the angle/state.
 *
 * computeAt() is the pure, stateless math behind it — no QObject, no cave/trip
 * dependency. Resolves (location, sourceCS) → degrees east-positive (positive =
 * grid north lies east of true north). Geographic CRS short-circuits to 0 since
 * there's no grid.
 *
 * The grid in question is always the project's local projection: cavern solves
 * under *cs out and reports the stations in it, so that is the grid the plotted
 * north belongs to. A fix station's own inputCS() says where the cave is, never
 * which grid the answer is about — converging to it would rotate the notes by
 * some other projection's angle (UTM's ±1°) against stations that were never
 * plotted in it.
 *
 * Convergence depends on location even within one projection, which is why the
 * readout is per-cave: in the local projection it is 0 at the anchor and grows
 * with distance from it (~0.4° at the 50 km the frame allows). No date
 * dependency — unlike magnetic declination, grid convergence is a fixed
 * property of the projection. Magnetic declination is NOT folded in here; that
 * lives in cwDeclination.
 */
class CAVEWHERE_LIB_EXPORT cwGridConvergence : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(GridConvergence)
    QML_UNCREATABLE("Accessed through cwCave::gridConvergence")

    Q_PROPERTY(double angle READ angle NOTIFY changed)
    Q_PROPERTY(State state READ state NOTIFY changed)
    Q_PROPERTY(QString text READ text NOTIFY changed)
    Q_PROPERTY(QString detailText READ detailText NOTIFY changed)

public:
    /// Whether angle() is meaningful, and if not, why. QML formats the readout
    /// off this; the n/a wording lives in text()/detailText().
    enum State {
        Valid,                ///< angle() holds a real convergence
        NoFixStation,         ///< no fix station places the cave in the frame
        NoCoordinateSystem,   ///< the project has no local projection
        Error                 ///< PROJ failed (unknown/invalid CS)
    };
    Q_ENUM(State)

    explicit cwGridConvergence(QObject* parent = nullptr);

    /// Cached convergence in degrees (PROJ east-positive). 0.0 unless Valid.
    double angle() const { return m_angle; }

    /// Why the convergence is (or isn't) available.
    State state() const { return m_state; }

    /// Compact readout, e.g. "0.74° at a1", or "n/a (no fix station)".
    QString text() const;

    /// text() plus the grid it is measured in, for tooltip use. Matches text()
    /// for n/a cases (nothing extra to surface).
    QString detailText() const;

    /// Recompute in the project's frame \a frameCS, at the location of the
    /// first fix station that places the cave in it. A project with no frame
    /// reads NoCoordinateSystem — nothing stands in for one.
    void update(const QList<cwFixStation>& fixStations, const QString& frameCS);

    /// Pure, stateless math: (location, sourceCS) → convergence degrees.
    static Monad::Result<double> computeAt(const cwGeoPoint& location,
                                           const QString& sourceCS);

signals:
    void changed();

private:
    double m_angle = 0.0;
    State m_state = NoFixStation;
    QString m_station;
    QString m_error;
};

#endif // CWGRIDCONVERGENCE_H
