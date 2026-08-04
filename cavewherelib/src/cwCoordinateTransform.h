/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCOORDINATETRANSFORM_H
#define CWCOORDINATETRANSFORM_H

//Qt includes
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>
#include <memory>
#include <optional>

//Our includes
#include "cwGeoPoint.h"
#include "cwGlobals.h"

/**
 * Wraps a single PROJ transform between two coordinate systems.
 *
 * Thread safety: PJ_CONTEXT is per-instance and not thread-safe. A single
 * cwCoordinateTransform is safe for use by ONE thread at a time. Construct
 * one per worker (LAZ loader, line-plot post-processor, GUI). If many
 * threads must share a single transform, switch to proj_clone_pj() per
 * worker — not v1.
 *
 * Same-CS short-circuit: if srcCS == dstCS (case-insensitive trimmed
 * compare), isIdentity() is true and transform()/transformInPlace() skip
 * the proj call entirely. Useful for LAZ loaders whose source already
 * matches globalCS.
 */
class CAVEWHERE_LIB_EXPORT cwCoordinateTransform
{
public:
    cwCoordinateTransform(const QString& srcCS, const QString& dstCS);
    ~cwCoordinateTransform();

    cwCoordinateTransform(const cwCoordinateTransform&) = delete;
    cwCoordinateTransform& operator=(const cwCoordinateTransform&) = delete;
    cwCoordinateTransform(cwCoordinateTransform&& other) noexcept;
    cwCoordinateTransform& operator=(cwCoordinateTransform&& other) noexcept;

    bool isValid() const;
    bool isIdentity() const;
    QString errorMessage() const;

    cwGeoPoint transform(const cwGeoPoint& src) const;
    void transformInPlace(cwGeoPoint* pts, qsizetype n) const;

    // Canonical WGS84 geographic CRS string. Prefer this over the literal
    // "EPSG:4326" so the value lives in exactly one place.
    static const QString Wgs84;

    static QStringList commonProjectedCSList();
    static bool isValidCS(const QString& cs);
    static bool isGeographic(const QString& cs);

    /**
     * Transform a point between two systems, memoizing the built transforms
     * per thread (same pattern and cap as isValidCS). Building one costs
     * proj_create_crs_to_crs — a proj.db query and a pipeline build, the most
     * expensive PROJ call there is — while callers like
     * cwLocalProjectionManager ask this on every keystroke in a coordinate
     * field.
     *
     * Empty when either system is empty, the two can't be related, or the
     * result's x/y isn't finite — an unanswerable question must not read as an
     * answer. z passes through untouched by the finiteness check.
     */
    static std::optional<cwGeoPoint> transformPoint(const QString& sourceCS,
                                                    const QString& destCS,
                                                    const cwGeoPoint& point);

    /**
     * Which horizontal components of `point` (in cs's own axis order and units)
     * fall outside cs's declared area of use — i.e. whether it inverse-projects
     * to a geographic location within the CRS's valid domain, widened by a small
     * margin. `eastingValid`/`northingValid` are true when the corresponding axis
     * (longitude for the easting, latitude for the northing) is inside, so a
     * caller can tint just the offending cell; an axis goes false only when PROJ
     * can evaluate the CRS and places the point well outside that domain — the
     * signature of a transposed digit, wrong UTM zone, or wrong hemisphere.
     *
     * Both stay true — never flags — when cs is empty or unparseable or its area
     * of use is unknown, so an un-checkable CS defers to the cluster rule rather
     * than crying wolf. z is not part of the domain test, so elevation is never
     * reported.
     *
     * Callers judging a *fix station* should go through
     * cwFixStationDiagnostics::domainCheck instead, which resolves the fix's
     * effective CS first — that resolution is the rule, and skipping it judges a
     * fix under the wrong CS.
     *
     * Attribution is best-effort: a coordinate whose inverse projection wraps
     * (a northing far past the pole flips the longitude ~180°) can't be blamed
     * on one axis, so both are reported invalid rather than the wrong one.
     */
    struct DomainCheck {
        bool eastingValid = true;
        bool northingValid = true;
    };
    static DomainCheck domainCheck(const QString& cs, const cwGeoPoint& point);

    static QString utmZoneToEpsg(int zone, bool north);
    static QString nameFor(const QString& cs);

    /**
     * Derive a *projected* coordinate system usable as the region's global
     * (output) CS from a single fix's input CS and coordinate. survex/cavern
     * only emits projected output, so a geographic input can't seed the global
     * CS directly:
     *   - inputCS already valid and projected -> returned unchanged.
     *   - inputCS geographic -> the WGS84 UTM zone containing the fix.
     *   - inputCS empty/invalid, or nothing projected can be derived -> "".
     * `point` is in inputCS's own axis order and units.
     */
    static QString deriveProjectedOutputCS(const QString& inputCS, const cwGeoPoint& point);

    /**
     * Set the directories PROJ searches for proj.db and grid-shift files.
     * Should be called once at application startup (before any
     * cwCoordinateTransform / isValidCS call) with the result of
     * cwGlobals::projDataPath(). Subsequent PROJ contexts created by this
     * class apply these paths via proj_context_set_search_paths().
     */
    static void setProjSearchPaths(const QStringList& paths);

private:
    // Pimpl hides <proj.h> from the public include path so consumers that
    // link cavewherelib but don't see PROJ::proj's include dirs (e.g. the
    // test executable) don't need to find proj.h. Sibling .cpp files that
    // do need PROJ types include cwCoordinateTransformPrivate.h.
    std::unique_ptr<class cwCoordinateTransformPrivate> m_d;
};

/**
 * QML-facing facade for cwCoordinateTransform's static helpers.
 *
 * Registered as a singleton so QML can call:
 *   CoordinateSystem.isValidCS(text)
 *   CoordinateSystem.commonProjectedCSList()
 *
 * The C++ cwCoordinateTransform itself is non-QObject (it owns PROJ
 * resources and is move-only), so it can't be exposed directly.
 */
class CAVEWHERE_LIB_EXPORT cwCoordinateSystem : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CoordinateSystem)
    QML_SINGLETON

public:
    explicit cwCoordinateSystem(QObject* parent = nullptr) : QObject(parent) {}

    /**
     * Picker modes. Custom is the escape hatch that opens the CSCustomDialog.
     *
     * Which modes a picker offers is per-host: the project's global CS keeps
     * Local (a cave that isn't georeferenced has to be able to say so) and
     * never offers Project; a fix-station row is the mirror image.
     *
     * Project is an action, not a stored mode — it stamps the project's global
     * CS into the row, which then holds it whatever the project does later. So
     * modeFor() can never return it: the CS string alone doesn't say whether
     * it was picked that way, and the picker decides by comparing against the
     * project's CS.
     */
    enum Mode { Local, LatLon, UTM, Custom, Project };
    Q_ENUM(Mode)

    Q_INVOKABLE static bool isValidCS(const QString& cs);
    Q_INVOKABLE static QStringList commonProjectedCSList();

    /// QML-facing alias for cwCoordinateTransform::Wgs84.
    Q_INVOKABLE static QString wgs84();

    /**
     * True iff cs parses as a geographic CRS (lat/long), e.g. EPSG:4326.
     * Used by the picker to keep geographic systems out of region-level
     * globalCS — survex's cavern only emits projected output.
     */
    Q_INVOKABLE static bool isGeographic(const QString& cs);

    /**
     * Build the WGS84 UTM EPSG code for a zone (1..60) and hemisphere.
     * North → EPSG:326NN, south → EPSG:327NN. Returns "" if zone is out of
     * range. Pure arithmetic — no PROJ call.
     */
    Q_INVOKABLE static QString utmZoneToEpsg(int zone, bool north);

    /**
     * Round-trip a CS string back to a picker mode (Local / LatLon / UTM /
     * Custom — never Project, see the enum). Splitting the parse into three
     * Q_INVOKABLEs lets QML bind each slice as a strict-typed property.
     * utmZoneFor returns -1 and utmNorthFor returns true when mode is not UTM.
     */
    Q_INVOKABLE static Mode modeFor(const QString& cs);
    Q_INVOKABLE static int  utmZoneFor(const QString& cs);
    Q_INVOKABLE static bool utmNorthFor(const QString& cs);

    /**
     * Human-readable description for a CS (e.g. "OSGB36 / British National
     * Grid" for "EPSG:27700"). Returns "" for empty/invalid strings.
     */
    Q_INVOKABLE static QString nameFor(const QString& cs);
};

#endif // CWCOORDINATETRANSFORM_H
