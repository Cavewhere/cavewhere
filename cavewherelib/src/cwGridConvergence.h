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
 * Convergence depends on location even within one projection: inside UTM Zone
 * 13N it varies by ~1.7° between the central meridian and the zone edge, so the
 * caller picks a representative point (e.g. a cave's first fix station). No date
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
        NoFixStation,         ///< cave has no fix station to anchor a location
        NoCoordinateSystem,   ///< no input CS and no region globalCS
        Geographic,           ///< CS is geographic, so there's no grid
        Error                 ///< PROJ failed (unknown/invalid CS)
    };
    Q_ENUM(State)

    explicit cwGridConvergence(QObject* parent = nullptr);

    /// Cached convergence in degrees (PROJ east-positive). 0.0 unless Valid.
    double angle() const { return m_angle; }

    /// Why the convergence is (or isn't) available.
    State state() const { return m_state; }

    /// Compact readout, e.g. "0.74° at a1", or "n/a (geographic CS)".
    QString text() const;

    /// text() plus the CS name for tooltip use, e.g. "0.74° at a1 (UTM 13N)".
    /// Matches text() for n/a cases (nothing extra to surface).
    QString detailText() const;

    /// Recompute from the cave's fix stations, falling back to the region's
    /// coordinate system when the first fix station has no input CS of its own.
    void update(const QList<cwFixStation>& fixStations, const QString& fallbackCoordinateSystem);

    /// Pure, stateless math: (location, sourceCS) → convergence degrees.
    static Monad::Result<double> computeAt(const cwGeoPoint& location,
                                           const QString& sourceCS);

signals:
    void changed();

private:
    double m_angle = 0.0;
    State m_state = NoFixStation;
    QString m_station;
    QString m_coordinateSystem;
    QString m_error;
};

#endif // CWGRIDCONVERGENCE_H
