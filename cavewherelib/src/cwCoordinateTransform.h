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
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <memory>

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

    static QStringList commonProjectedCSList();
    static bool isValidCS(const QString& cs);
    static bool isGeographic(const QString& cs);
    static QString utmZoneToEpsg(int zone, bool north);
    static QVariantMap parseCSMode(const QString& cs);
    static QString nameFor(const QString& cs);

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

    Q_INVOKABLE static bool isValidCS(const QString& cs);
    Q_INVOKABLE static QStringList commonProjectedCSList();

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
     * Round-trip a CS string back to a picker mode. Returns a map with key
     * "mode" set to one of "local", "latlon", "utm", "custom"; for "utm"
     * the map also contains "utmZone" (int) and "utmNorth" (bool); "raw"
     * always carries the original cs string.
     */
    Q_INVOKABLE static QVariantMap parseCSMode(const QString& cs);

    /**
     * Field-at-a-time slices of parseCSMode() so QML callers can use
     * strict-typed properties instead of `property var`.
     */
    Q_INVOKABLE static QString modeFor(const QString& cs);
    Q_INVOKABLE static int     utmZoneFor(const QString& cs);
    Q_INVOKABLE static bool    utmNorthFor(const QString& cs);

    /**
     * Human-readable description for a CS (e.g. "OSGB36 / British National
     * Grid" for "EPSG:27700"). Returns "" for empty/invalid strings.
     */
    Q_INVOKABLE static QString nameFor(const QString& cs);
};

#endif // CWCOORDINATETRANSFORM_H
