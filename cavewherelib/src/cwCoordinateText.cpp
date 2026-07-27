/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwCoordinateText.h"

//Our includes
#include "cwCoordinateTransform.h"

//Qt includes
#include <QLocale>
#include <QRegularExpression>
#include <QStringView>

//Std includes
#include <algorithm>
#include <cmath>

namespace {

//! One number plus an optional unit suffix. The suffix group is optional as a
//! whole rather than matching empty: a plain `\s*[A-Za-z]*` tail would eat the
//! space in "46.12 -115.59" and leave the two numbers looking adjacent, which
//! the separator check would then reject.
const QRegularExpression& componentExpression()
{
    static const QRegularExpression expression(
        QStringLiteral(R"(([+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?)(?:\s*([A-Za-z]+))?)"));
    return expression;
}

//! What may sit between two components — commas and whitespace, nothing else.
bool isSeparator(QStringView text)
{
    return std::all_of(text.cbegin(), text.cend(), [](QChar character) {
        return character.isSpace() || character == u',';
    });
}

constexpr qsizetype kMinComponents = 2;
constexpr qsizetype kMaxComponents = 3;

//! An example in the order being read, so the message a user gets back
//! demonstrates the arrangement their own row wants rather than the other one.
QString exampleFor(cwCoordinateText::AxisOrder order)
{
    return order == cwCoordinateText::LatitudeLongitude
               ? QStringLiteral("46.12113, -115.59902, 304m")
               : QStringLiteral("610016.792, 5615117.075, 2545.34m");
}

//! The shortest text that reads back as the same double, so re-committing an
//! untouched field can't drift the coordinate and 46.12113 still renders as
//! "46.12113" — a fixed precision has to choose one or the other (15 digits
//! loses the low bits, 17 renders it "46.121129999999997").
QString shortestNumber(double value)
{
    //'f' rather than 'g': both round-trip, but 'g' switches to scientific for
    //round magnitudes, so a UTM easting of exactly 500000 would render "5e+05"
    //in a field the user types into. Coordinates are bounded, so fixed notation
    //can't run away.
    return QLocale::c().toString(value, 'f', QLocale::FloatingPointShortest);
}

}

cwCoordinateText::AxisOrder cwCoordinateText::axisOrderFor(const QString& cs)
{
    return cwCoordinateTransform::isGeographic(cs) ? LatitudeLongitude : EastingNorthing;
}

Monad::Result<cwCoordinateText::Coordinate>
cwCoordinateText::parse(const QString& text, cwUnits::UnitSystem units, AxisOrder order)
{
    using ParseResult = Monad::Result<Coordinate>;

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return ParseResult(tr("Type a coordinate, for example \"%1\".").arg(exampleFor(order)));
    }

    struct Component {
        double value = 0.0;
        QString unit;
    };

    QList<Component> components;
    qsizetype cursor = 0;

    QRegularExpressionMatchIterator iterator = componentExpression().globalMatch(trimmed);
    while (iterator.hasNext()) {
        const QRegularExpressionMatch match = iterator.next();
        const QStringView gap = QStringView(trimmed).mid(cursor, match.capturedStart() - cursor);

        if (!isSeparator(gap)) {
            return ParseResult(tr("Couldn't read \"%1\" as part of a coordinate.")
                                   .arg(gap.trimmed().toString()));
        }
        //Components have to be separated by something. "46.12-115.6" reads as
        //two numbers to the expression above, but it is a typo far more often
        //than it is a coordinate.
        if (cursor > 0 && gap.isEmpty()) {
            return ParseResult(tr("Separate the numbers with commas or spaces, "
                                  "for example \"%1\".").arg(exampleFor(order)));
        }

        bool ok = false;
        const double value = match.captured(1).toDouble(&ok);
        if (!ok || !std::isfinite(value)) {
            return ParseResult(tr("\"%1\" is too big to be a coordinate.").arg(match.captured(1)));
        }

        components.append(Component{value, match.captured(2)});
        cursor = match.capturedEnd();
    }

    const QStringView tail = QStringView(trimmed).mid(cursor);
    if (!isSeparator(tail)) {
        return ParseResult(tr("Couldn't read \"%1\" as part of a coordinate.")
                               .arg(tail.trimmed().toString()));
    }

    if (components.size() < kMinComponents) {
        return ParseResult(order == LatitudeLongitude
                               ? tr("A coordinate needs a latitude and a longitude, "
                                    "for example \"%1\".").arg(exampleFor(order))
                               : tr("A coordinate needs an easting and a northing, "
                                    "for example \"%1\".").arg(exampleFor(order)));
    }
    if (components.size() > kMaxComponents) {
        return ParseResult(tr("A coordinate takes at most three numbers "
                              "(easting, northing, elevation) — this has %1.")
                               .arg(components.size()));
    }

    //Only the last of three components is an elevation; with two, both are
    //horizontal and neither may carry a unit. "Horizontal" is either
    //easting/northing or latitude/longitude — which is why order matters.
    const bool hasElevation = (components.size() == kMaxComponents);
    const qsizetype horizontalCount = hasElevation ? components.size() - 1 : components.size();
    for (qsizetype i = 0; i < horizontalCount; i++) {
        const QString& unit = components.at(i).unit;
        if (!unit.isEmpty()) {
            return ParseResult(order == LatitudeLongitude
                                   ? tr("Only the elevation can carry a unit — the latitude and "
                                        "longitude are degrees. Remove \"%1\".").arg(unit)
                                   : tr("Only the elevation can carry a unit — the easting and "
                                        "northing are already in their coordinate system's own "
                                        "units. Remove \"%1\".").arg(unit));
        }
    }

    Coordinate coordinate;
    if (order == LatitudeLongitude) {
        coordinate.northing = components.at(0).value;
        coordinate.easting = components.at(1).value;
    } else {
        coordinate.easting = components.at(0).value;
        coordinate.northing = components.at(1).value;
    }

    coordinate.hasElevation = hasElevation;
    if (hasElevation) {
        const Component& elevation = components.at(kMaxComponents - 1);
        coordinate.hasElevationUnit = !elevation.unit.isEmpty();
        const cwUnits::LengthUnit unit = elevation.unit.isEmpty()
                                             ? elevationUnit(units)
                                             : cwUnits::toLengthUnit(elevation.unit);
        if (unit == cwUnits::LengthUnitless) {
            return ParseResult(tr("\"%1\" isn't a length unit — try m, ft, yd, in, mm, cm, "
                                  "km or mi.").arg(elevation.unit));
        }
        coordinate.elevation = cwUnits::convert(elevation.value, unit, cwUnits::Meters);
    }

    return ParseResult(coordinate);
}

QString cwCoordinateText::format(double easting,
                                 double northing,
                                 double elevationInMeters,
                                 cwUnits::UnitSystem units,
                                 AxisOrder order)
{
    const cwUnits::LengthUnit unit = elevationUnit(units);
    const double elevation = cwUnits::convert(elevationInMeters, cwUnits::Meters, unit);

    const double first = (order == LatitudeLongitude) ? northing : easting;
    const double second = (order == LatitudeLongitude) ? easting : northing;

    return QStringLiteral("%1, %2, %3%4")
        .arg(shortestNumber(first),
             shortestNumber(second),
             shortestNumber(elevation),
             cwUnits::unitName(unit));
}

QString cwCoordinateText::textToStore(const QString& text,
                                      const Coordinate& coordinate,
                                      cwUnits::UnitSystem units)
{
    const QString trimmed = text.trimmed();
    if (!coordinate.hasElevation || coordinate.hasElevationUnit) {
        return trimmed;
    }
    return trimmed + cwUnits::unitName(elevationUnit(units));
}

cwCoordinateTextValidator::cwCoordinateTextValidator(QObject* parent) :
    cwValidator(parent)
{
}

void cwCoordinateTextValidator::setAxisOrder(cwCoordinateText::AxisOrder order)
{
    if (m_axisOrder == order) {
        return;
    }
    m_axisOrder = order;
    emit axisOrderChanged();
}

QValidator::State cwCoordinateTextValidator::validate(QString& input, int& position) const
{
    Q_UNUSED(position);

    //The unit system stands in as Metric because it truly cannot change the
    //verdict: it only resolves what a bare elevation means, and every unit
    //system resolves it to something readable. The axis order is passed for
    //real — it picks the axis names and the example in the message below.
    const auto result = cwCoordinateText::parse(input, cwUnits::Metric, m_axisOrder);
    if (!result.hasError()) {
        return QValidator::Acceptable;
    }

    //QValidator::validate() is const by contract, but why a coordinate didn't
    //parse is a pure function of the input just handed to us, and
    //CoreClickTextInput reads errorText immediately after this returns.
    const_cast<cwCoordinateTextValidator*>(this)->setErrorText(result.errorMessage());
    return QValidator::Intermediate;
}

int cwCoordinateTextValidator::validate(QString input) const
{
    int position = 0;
    return static_cast<int>(validate(input, position));
}
