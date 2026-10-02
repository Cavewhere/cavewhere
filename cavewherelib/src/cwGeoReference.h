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

//Std includes
#include <optional>

//Our includes
#include "cwGeoPoint.h"
#include "cwGlobals.h"

/**
 * The geo-reference slice of a region: the project's local projection (LDP),
 * which is the frame the scene is in. Extracted from cwCavingRegion so
 * interactions that only need this slice (the azimuth reference, the coordinate
 * pick) can take a cwGeoReference* instead of the whole region.
 *
 * cwCavingRegion owns one and exposes it as region.geoReference — it is the
 * single home for this state, not a copy the region also mirrors. The
 * region-specific plumbing stays on the region: pushing the frame into the LAZ
 * layer model happens there and drives this slice through its public setters.
 *
 * There is no separate scene offset. The LDP is a transverse Mercator centered
 * on the project's anchor with x_0 = y_0 = 0, so a point's LDP coordinates
 * already are its scene coordinates — there is nothing left to subtract, and
 * nothing that can disagree about how much was subtracted. See
 * plans/LDP_AUTO_COORDINATE_SYSTEM_PLAN.html.
 */
class CAVEWHERE_LIB_EXPORT cwGeoReference : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(GeoReference)
    QML_UNCREATABLE("Owned by CavingRegion; access via region.geoReference")

    Q_PROPERTY(bool hasCoordinateSystem READ hasCoordinateSystem NOTIFY localProjectionChanged)
    Q_PROPERTY(QString localCoordinateSystem READ localCoordinateSystem NOTIFY localProjectionChanged)
    Q_PROPERTY(State state READ state NOTIFY localProjectionChanged)
    Q_PROPERTY(QString verticalDatum READ verticalDatum WRITE setVerticalDatum NOTIFY verticalDatumChanged)
    Q_PROPERTY(QString datumName READ datumName NOTIFY localProjectionChanged)
    Q_PROPERTY(bool hasOrigin READ hasOrigin NOTIFY localProjectionChanged)
    Q_PROPERTY(double originLatitude READ originLatitude NOTIFY localProjectionChanged)
    Q_PROPERTY(double originLongitude READ originLongitude NOTIFY localProjectionChanged)

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

    //! Single definition of "this is georeferenced": there is a frame, so scene
    //! points can be placed in a real-world CRS (true/magnetic north, WGS84,
    //! export). Consumers should ask this rather than re-deriving the rule from
    //! the state or the string.
    bool hasCoordinateSystem() const { return state() != Ungeoreferenced; }

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

    //! Move the frame to \a localCS, leaving it Frozen and anchorless. Two
    //! callers, one meaning: the frame's position stops being answerable to any
    //! single input — either every input drifted far from the origin and the
    //! automatic path is correcting a frame that outlived what placed it, or
    //! the user asked for the middle of the data, which no one input can claim.
    //! Legal from Anchored as well as Frozen. State and string are written
    //! together, and an empty \a localCS is refused the way anchorTo() refuses
    //! one.
    void recenter(const QString& localCS);

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

    //! The name of the datum the frame is on, as a reader recognizes it ("North
    //! American Datum 1983"). Inherited from whatever anchored the project, so
    //! it describes the frame rather than offering a choice — see D4 in
    //! plans/LDP_AUTO_COORDINATE_SYSTEM_PLAN.html. Empty without a frame.
    QString datumName() const { return m_datumName; }

    //! Whether the frame reports where it is centered. False without a frame.
    bool hasOrigin() const { return m_origin.has_value(); }

    //! Where the frame is centered, in degrees on its own datum. Both read 0
    //! when hasOrigin() is false.
    double originLatitude() const { return m_origin.has_value() ? m_origin->y : 0.0; }
    double originLongitude() const { return m_origin.has_value() ? m_origin->x : 0.0; }

signals:
    //! The LDP, the state, or the anchor changed — they move together, so they
    //! report together.
    void localProjectionChanged();

    //! The frame itself moved: localCoordinateSystem() now returns a different
    //! string. Narrower than localProjectionChanged, which also reports a
    //! freeze or a change of anchor — both of which leave every coordinate in
    //! the project exactly where it was. Whoever has to redo work in the frame
    //! (the point clouds re-decode) listens here; whoever describes the frame
    //! listens to localProjectionChanged. Emitted first, so the frame's
    //! dependents are up to date before its describers run.
    void localCoordinateSystemChanged();

    void verticalDatumChanged();

private:
    // The three parts of the local projection, bundled: a state, the string it
    // stands for, and the input it came from are only meaningful together, and
    // every path that writes one writes all three.
    struct LocalProjectionState {
        State state = Ungeoreferenced;
        QString coordinateSystem;
        Anchor anchor;

        bool operator==(const LocalProjectionState& other) const = default;
    };
    LocalProjectionState m_localProjection;

    QString m_verticalDatum;

    // What the frame says about itself, read out of it once when it changes
    // rather than on every binding that describes it. setLocalProjection is the
    // only writer of the frame, so it is the only writer of these.
    QString m_datumName;
    std::optional<cwGeoPoint> m_origin;

    void setLocalProjection(const LocalProjectionState& state);

    //! Become Frozen at \a localCS. What freeze() and recenter() both are, and
    //! the one place that says what a frozen frame is made of: a string, no
    //! anchor, and an empty string refused rather than half-applied.
    void freezeTo(const QString& localCS);
};

#endif // CWGEOREFERENCE_H
