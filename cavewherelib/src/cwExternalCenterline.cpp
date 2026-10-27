/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwExternalCenterline.h"
#include "cwCoordinateText.h"
#include "cwCoordinateTransform.h"
#include "cwSurvexCS.h"

//Qt includes
#include <QRegularExpression>

namespace {

//! How many numbers lead a Survex *fix's coordinate: x, y and z. Cavern
//! takes standard errors after them, which play no part in the position.
constexpr qsizetype kFixCoordinateComponents = 3;

//! \a system read the way the driver passes it after *cs: a Survex name maps
//! through cavern's own reading of it, and anything else is taken as PROJ text.
struct ResolvedSystem
{
    QString cs;
    bool latitudeFirst = false;
};

ResolvedSystem resolveSystem(const QString& system)
{
    const std::optional<cwSurvexCS::Parsed> parsed = cwSurvexCS::fromSurvexCS(system);
    if (parsed.has_value()) {
        return {parsed->projCS, parsed->latitudeFirst};
    }
    return {system, false};
}

} // namespace

cwExternalCenterline::cwExternalCenterline(QString entryFile)
    : m_entryFile(std::move(entryFile))
{
}

size_t qHash(const cwExternalCenterline& value, size_t seed) noexcept
{
    return qHash(value.entryFile(), seed);
}

QString cwAttachedFix::readInWarning() const
{
    if (readInSystem.isEmpty()) {
        return QString();
    }
    const QString where = line > 0
        ? QStringLiteral("%1 line %2").arg(filePath).arg(line)
        : filePath;
    const QString source = readInFileOutput
        ? QStringLiteral("the system the file's *cs out names")
        : QStringLiteral("the system chosen for %1").arg(fileName);
    return QStringLiteral("Read in %1, %2: %3 names no coordinate system for this fix.")
        .arg(readInSystem, source, where);
}

QString cwAttachedFix::solveSystem() const
{
    if (!placementSystem.isEmpty()) {
        return placementSystem;
    }
    return coordinateSystem.isEmpty() ? readInSystem : coordinateSystem;
}

QString cwAttachedFix::systemLabel() const
{
    return placementSystem.isEmpty()
        ? coordinateSystem
        : QStringLiteral("%1 (%2)").arg(coordinateSystem, placementSystem);
}

std::optional<cwAttachedFix::Placement> cwAttachedFix::placementIn(const QString& system) const
{
    const ResolvedSystem resolved = resolveSystem(system);
    if (!cwCoordinateTransform::isValidCS(resolved.cs)) {
        return std::nullopt;
    }

    const std::optional<cwGeoPoint> read = positionOf(coordinate);
    if (!read.has_value()) {
        return std::nullopt;
    }
    const cwGeoPoint point(resolved.latitudeFirst ? read->y : read->x,
                           resolved.latitudeFirst ? read->x : read->y,
                           read->z);
    return Placement{resolved.cs, point};
}

std::optional<cwGeoPoint> cwAttachedFix::gridPosition() const
{
    if (cwCoordinateTransform::isGeographic(resolveSystem(solveSystem()).cs)) {
        return std::nullopt;
    }
    return positionOf(coordinate);
}

std::optional<cwGeoPoint> cwAttachedFix::positionOf(const QString& coordinate)
{
    const QString position =
        coordinate.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts)
            .mid(0, kFixCoordinateComponents)
            .join(QLatin1Char(' '));
    const auto parsedCoordinate = cwCoordinateText::parse(position, cwUnits::Metric,
                                                          cwCoordinateText::EastingNorthing);
    if (parsedCoordinate.hasError()) {
        return std::nullopt;
    }
    const cwCoordinateText::Coordinate read = parsedCoordinate.value();
    return cwGeoPoint(read.easting, read.northing, read.elevation);
}
