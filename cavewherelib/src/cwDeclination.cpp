/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwDeclination.h"
#include "cwCoordinateTransform.h"

//Survex include — thgeomag is a C function, wrap to suppress C++ mangling
extern "C" {
#include "thgeomag.h"
}

//Qt includes
#include <QDate>
#include <QHash>
#include <QtMath>

//Std includes
#include <memory>

namespace {

double decimalYear(const QDateTime& dt)
{
    const QDate d = dt.date();
    const int year = d.year();
    const QDate yearStart(year, 1, 1);
    const double dayFraction = dt.time().msecsSinceStartOfDay() / 86400000.0;
    const double dayInYear = yearStart.daysTo(d) + dayFraction;
    const double yearDays = QDate::isLeapYear(year) ? 366.0 : 365.0;
    return year + dayInYear / yearDays;
}

// Caches one transform per sourceCS string. compute() is on a hot path:
// trip-table renders, scrap updates, and per-trip signal cascades can call
// it many times in succession with the same sourceCS. proj_create_crs_to_crs
// hits proj.db, so re-running it per call is the main avoidable cost.
// thread_local mirrors cwCoordinateTransform's validatorContext() pattern
// since PJ_CONTEXT is per-instance and not thread-safe.
const cwCoordinateTransform& transformToWgs84(const QString& sourceCS)
{
    // shared_ptr (not unique_ptr) because QHash requires copyable values, and
    // cwCoordinateTransform is move-only — see the deleted copy ctor in
    // cwCoordinateTransform.h.
    thread_local QHash<QString, std::shared_ptr<cwCoordinateTransform>> cache;
    auto it = cache.constFind(sourceCS);
    if (it == cache.constEnd()) {
        if (cache.size() >= 256) {
            cache.clear();
        }
        it = cache.insert(sourceCS,
                          std::make_shared<cwCoordinateTransform>(sourceCS, cwCoordinateTransform::Wgs84));
    }
    return **it;
}

} // namespace

namespace cwDeclination {

Monad::Result<double> compute(const cwGeoPoint& location,
                              const QString& sourceCS,
                              const QDateTime& date)
{
    if (!date.isValid()) {
        return Monad::Result<double>(
            QStringLiteral("Trip date is missing or invalid"));
    }

    const QString normalizedCS = sourceCS.trimmed();
    if (normalizedCS.isEmpty()) {
        return Monad::Result<double>(
            QStringLiteral("Source coordinate system is empty"));
    }

    // cwCoordinateTransform's normalize_for_visualization step makes
    // EPSG:4326 deliver (x=lon, y=lat) regardless of CRS metadata.
    cwGeoPoint wgs84Point;
    if (normalizedCS.compare(cwCoordinateTransform::Wgs84, Qt::CaseInsensitive) == 0) {
        wgs84Point = location;
    } else {
        const cwCoordinateTransform& transform = transformToWgs84(normalizedCS);
        if (!transform.isValid()) {
            return Monad::Result<double>(
                QStringLiteral("Failed to transform from '%1' to WGS84: %2")
                    .arg(normalizedCS, transform.errorMessage()));
        }
        wgs84Point = transform.transform(location);
    }

    const double lonRad = qDegreesToRadians(wgs84Point.x);
    const double latRad = qDegreesToRadians(wgs84Point.y);
    const double elevMeters = wgs84Point.z;
    const double decYear = decimalYear(date);

    const double declRadians = thgeomag(latRad, lonRad, elevMeters, decYear);
    return Monad::Result<double>(qRadiansToDegrees(declRadians));
}

} // namespace cwDeclination
