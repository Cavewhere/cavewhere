/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGEOREFERENCE_H
#define CWGEOREFERENCE_H

//Qt includes
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QUuid>
#include <QVector3D>

//Our includes
#include "cwGlobals.h"
#include "cwGeoPoint.h"

/**
 * The geo-reference slice of a region: the project coordinate system plus the
 * worldOrigin offset that together place scene-local points in a real-world
 * CRS. Extracted from cwCavingRegion so interactions that only need this slice
 * (the azimuth reference, the coordinate pick) can take a cwGeoReference*
 * instead of the whole region.
 *
 * cwCavingRegion owns one and exposes it as region.geoReference — it is the
 * single home for this state, not a copy the region also mirrors. The
 * region-specific plumbing stays on the region: pushing changes into the LAZ
 * layer model and the cave-based recomputeWorldOrigin() both live there and
 * drive this slice through its public setters.
 *
 * This class is mid-rework. It carries two frames at once: the
 * user-or-auto globalCoordinateSystem + worldOrigin pair that every consumer
 * still reads, and the automatically derived local projection (LDP) that will
 * replace both — stored here, persisted, and not yet wired to anything. See
 * plans/LDP_AUTO_COORDINATE_SYSTEM_PLAN.html.
 */
class CAVEWHERE_LIB_EXPORT cwGeoReference : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(GeoReference)
    QML_UNCREATABLE("Owned by CavingRegion; access via region.geoReference")

    Q_PROPERTY(QString globalCoordinateSystem READ globalCoordinateSystem WRITE setGlobalCoordinateSystem NOTIFY globalCoordinateSystemChanged)
    Q_PROPERTY(bool hasCoordinateSystem READ hasCoordinateSystem NOTIFY globalCoordinateSystemChanged)
    Q_PROPERTY(cwGeoPoint worldOrigin READ worldOrigin WRITE setWorldOrigin NOTIFY worldOriginChanged)
    Q_PROPERTY(QString localCoordinateSystem READ localCoordinateSystem NOTIFY localProjectionChanged)
    Q_PROPERTY(State state READ state NOTIFY localProjectionChanged)
    Q_PROPERTY(QString verticalDatum READ verticalDatum WRITE setVerticalDatum NOTIFY verticalDatumChanged)

public:
    /**
     * Where the project's local projection stands. The transitions between
     * these are the whole of the LDP's lifecycle — see the plan's state
     * machine; this class only holds the state and its invariants, and the
     * region drives it.
     */
    enum State {
        //! No LDP. Nothing georeferenced has been entered, so the cave renders
        //! in a floating local frame.
        Ungeoreferenced,
        //! An LDP derived from an anchor — the first georeferenced input — that
        //! still exists. Editing that one input far enough re-derives; deleting
        //! it hands off to Frozen or to a new anchor.
        Anchored,
        //! An LDP with no anchor left to follow. It moves only when the user
        //! asks for it explicitly.
        Frozen
    };
    Q_ENUM(State)

    /**
     * Which input placed the origin, by identity rather than by value — the
     * question the lifecycle asks is "was the thing just edited the anchor?",
     * and a coordinate can't answer that about itself.
     *
     * Both kinds carry a persisted UUID (cwFixStation::id, cwLazLayer::id), so
     * the identity survives save/load and outlives any rename.
     */
    struct Anchor {
        enum Kind {
            None,
            FixStation,
            LazLayer
        };

        Kind kind = None;
        QUuid id;

        bool isValid() const { return kind != None && !id.isNull(); }
        bool operator==(const Anchor& other) const = default;
    };

    explicit cwGeoReference(QObject* parent = nullptr);

    QString globalCoordinateSystem() const { return m_globalCoordinateSystem; }
    void setGlobalCoordinateSystem(const QString& cs);

    //! Single definition of "this is georeferenced": it has a coordinate system,
    //! so scene points can be placed in a real-world CRS (true/magnetic north,
    //! WGS84, export). Consumers should ask this rather than re-deriving the
    //! empty-CS rule.
    bool hasCoordinateSystem() const { return !m_globalCoordinateSystem.isEmpty(); }

    cwGeoPoint worldOrigin() const { return m_worldOrigin.value; }
    void setWorldOrigin(const cwGeoPoint& origin);

    // True iff worldOrigin was set explicitly (by the user, by load, by
    // recompute) — distinct from the default-constructed (0,0,0) that a fresh
    // reference carries. Used by cwLazLayerModel to decide whether an incoming
    // LAZ may auto-adopt its bbox center as the origin.
    bool hasExplicitWorldOrigin() const { return m_worldOrigin.explicitlySet; }

    //! Widen a worldOrigin-relative scene point back to a global cwGeoPoint in
    //! this reference's CRS. Thin wrapper over cwGeoPoint::fromSceneLocal so
    //! callers holding only the slice don't reach for the origin themselves.
    cwGeoPoint toGlobal(const QVector3D& sceneLocal) const {
        return cwGeoPoint::fromSceneLocal(sceneLocal, m_worldOrigin.value);
    }

    //! The project's derived local projection as a PROJ string, or "" in
    //! Ungeoreferenced. Always the stored string — never re-derived on read, so
    //! two machines with different PROJ data can't disagree about where the
    //! project is (cwLocalProjection).
    QString localCoordinateSystem() const { return m_localProjection.coordinateSystem; }

    State state() const { return m_localProjection.state; }

    //! Which input the LDP was derived from. Only meaningful in Anchored.
    Anchor anchor() const { return m_localProjection.anchor; }

    //! Derive-time transition to Anchored: \a localCS becomes the project's
    //! frame and \a anchor the input answerable for it. Both are written
    //! together — a state that named an anchor the string wasn't derived from
    //! would move the origin on the wrong edit.
    //!
    //! An invalid \a anchor or an empty \a localCS is refused outright rather
    //! than half-applied; callers with neither want clear().
    void anchorTo(const Anchor& anchor, const QString& localCS);

    //! Keep the LDP, give up the anchor: the input that placed the origin is
    //! gone, but the rest of the data is close enough that moving the frame
    //! would be churn. A no-op with no LDP to freeze.
    void freeze();

    //! Back to Ungeoreferenced — every georeferenced input is gone, so the
    //! frame it implied is too.
    void clear();

    //! Restore persisted state wholesale, repairing any combination that can't
    //! be true (an anchor with no string, a state naming an anchor it doesn't
    //! have). The load path, which must reproduce what was written rather than
    //! replay the transitions that produced it.
    void restore(State state, const QString& localCS, const Anchor& anchor,
                 const QString& verticalDatum);

    //! The name of the vertical datum the project's elevations are in (e.g.
    //! "NAVD88"), as declared by whatever supplied them. Recorded, never
    //! applied: z is a geoid height and stays one, and converting it to an
    //! ellipsoidal height would shift the whole project by the geoid
    //! separation. It is stored so a future globe renderer can reconstruct
    //! ellipsoidal height (h = H + N) when it needs one. Empty when nothing
    //! said.
    QString verticalDatum() const { return m_verticalDatum; }
    void setVerticalDatum(const QString& datum);

signals:
    void globalCoordinateSystemChanged();
    void worldOriginChanged();
    //! The LDP, the state, or the anchor changed — they move together, so they
    //! report together.
    void localProjectionChanged();
    void verticalDatumChanged();

private:
    QString m_globalCoordinateSystem;

    // Bundles the origin value with a flag tracking whether anyone has
    // explicitly chosen it. Glued together so the value and the flag can't
    // drift: every code path that mutates the value also touches the flag,
    // and the CS-change reset path (which is *not* a user choice) resets
    // both atomically.
    struct WorldOriginState {
        cwGeoPoint value;
        bool explicitlySet = false;
    };
    WorldOriginState m_worldOrigin;

    // The three parts of the local projection, bundled for the same reason as
    // the world origin above: a state, the string it stands for, and the input
    // it came from are only meaningful together, and every path that writes one
    // writes all three.
    struct LocalProjectionState {
        State state = Ungeoreferenced;
        QString coordinateSystem;
        Anchor anchor;

        bool operator==(const LocalProjectionState& other) const = default;
    };
    LocalProjectionState m_localProjection;

    QString m_verticalDatum;

    void setLocalProjection(const LocalProjectionState& state);
};

#endif // CWGEOREFERENCE_H
