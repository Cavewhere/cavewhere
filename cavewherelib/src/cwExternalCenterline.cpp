/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwExternalCenterline.h"
#include "cwCoordinateText.h"
#include "cwCoordinateTransform.h"
#include "cwExternalCenterlineScanner.h"
#include "cwSurvexCS.h"

//Qt includes
#include <QRegularExpression>

namespace {

//! How many numbers lead a Survex *fix's coordinate: x, y and z. Cavern
//! takes standard errors after them, which play no part in the position.
constexpr qsizetype kFixCoordinateComponents = 3;

} // namespace

cwExternalCenterline::cwExternalCenterline(QString entryFile)
    : m_entryFile(std::move(entryFile))
{
}

QString cwExternalCenterline::format() const
{
    return cwExternalCenterlineScanner::formatName(
        cwExternalCenterlineScanner::formatFor(m_entryFile));
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
    return coordinateSystem.isEmpty() ? readInSystem : coordinateSystem;
}

std::optional<cwAttachedFix::Placement> cwAttachedFix::placementIn(const QString& system) const
{
    // A Survex name maps through cavern's own reading of it; anything that
    // isn't one is taken as PROJ text, as the driver passes it on.
    const std::optional<cwSurvexCS::Parsed> parsed = cwSurvexCS::fromSurvexCS(system);
    const QString cs = parsed.has_value() ? parsed->projCS : system;
    if (!cwCoordinateTransform::isValidCS(cs)) {
        return std::nullopt;
    }

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
    const bool latitudeFirst = parsed.has_value() && parsed->latitudeFirst;
    const cwGeoPoint point(latitudeFirst ? read.northing : read.easting,
                           latitudeFirst ? read.easting : read.northing,
                           read.elevation);
    return Placement{cs, point};
}
