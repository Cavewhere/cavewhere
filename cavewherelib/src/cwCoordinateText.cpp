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

//! One number as it was written: what it says, the span it occupies, and any
//! word that followed it.
struct Number
{
    double value = 0.0;
    qsizetype start = 0;
    qsizetype end = 0;
    QString unit;
};

//! One component of a coordinate: what it says, the word written after it, and
//! the span of text its numbers occupy.
//!
//! groupNumbers() is the one place that decides how numbers combine into a
//! component, so a component arrives already knowing its own value rather than
//! leaving each reader to derive one from parts.
//!
//! The span covers the numbers alone. swapHorizontal() rewrites a coordinate by
//! exchanging two of these spans, and a unit belongs to the component it was
//! written on, so it stays where it was written.
struct Component
{
    double value = 0.0;
    QString unit;
    qsizetype start = 0;
    qsizetype end = 0;

    qsizetype length() const { return end - start; }
};

//! Every number \a text spells out, or the reason something between two of them
//! isn't a thing a coordinate may contain.
//!
//! Offsets are into \a text as given, leading and trailing whitespace and all,
//! so a caller that rewrites the string can leave the user's own spacing where
//! they put it. Scanning untrimmed reads the same numbers either way: whatever
//! surrounds them has to be a separator, and whitespace is one.
//!
//! \a order phrases the worked example in one message and decides nothing about
//! what gets read.
Monad::Result<QList<Number>> scanNumbers(const QString& text,
                                         cwCoordinateText::AxisOrder order)
{
    using ScanResult = Monad::Result<QList<Number>>;

    const auto unreadable = [](QStringView word) {
        return cwCoordinateText::tr("Couldn't read \"%1\" as part of a coordinate.")
            .arg(word.trimmed().toString());
    };

    QList<Number> numbers;
    qsizetype cursor = 0;

    QRegularExpressionMatchIterator iterator = componentExpression().globalMatch(text);
    while (iterator.hasNext()) {
        const QRegularExpressionMatch match = iterator.next();
        const QStringView gap = QStringView(text).mid(cursor, match.capturedStart() - cursor);

        if (!isSeparator(gap)) {
            return ScanResult(unreadable(gap));
        }
        //Numbers have to be separated by something. "46.12-115.6" reads as two
        //numbers to the expression above, but it is a typo far more often than
        //it is a coordinate.
        if (cursor > 0 && gap.isEmpty()) {
            return ScanResult(cwCoordinateText::tr("Separate the numbers with commas or spaces, "
                                                   "for example \"%1\".").arg(exampleFor(order)));
        }

        bool ok = false;
        const double value = match.captured(1).toDouble(&ok);
        if (!ok || !std::isfinite(value)) {
            return ScanResult(cwCoordinateText::tr("\"%1\" is too big to be a coordinate.")
                                  .arg(match.captured(1)));
        }

        numbers.append(Number{value,
                              match.capturedStart(1),
                              match.capturedEnd(1),
                              match.captured(2)});
        cursor = match.capturedEnd();
    }

    const QStringView tail = QStringView(text).mid(cursor);
    if (!isSeparator(tail)) {
        return ScanResult(unreadable(tail));
    }

    return ScanResult(numbers);
}

//! \a numbers assembled into the components they spell out.
//!
//! One number each: nothing a coordinate can say today joins two numbers into
//! one component. Degrees, minutes and seconds (#654) will be the first thing
//! that can, and this is where they will be joined — which is why what follows
//! counts and interprets components rather than numbers, and why deciding what
//! a component says is this function's job alone.
QList<Component> groupNumbers(const QList<Number>& numbers)
{
    QList<Component> components;
    components.reserve(numbers.size());
    for (const Number& number : numbers) {
        components.append(Component{number.value, number.unit, number.start, number.end});
    }
    return components;
}

//! The components \a text spells out, or the reason it isn't a coordinate.
//!
//! Everything decided here is decided from the text alone: what the components
//! mean — which axis each one is, what unit a bare elevation carries — is
//! readCoordinate()'s question, and \a order only phrases the messages.
Monad::Result<QList<Component>> readComponents(const QString& text,
                                               cwCoordinateText::AxisOrder order)
{
    using ComponentResult = Monad::Result<QList<Component>>;

    if (QStringView(text).trimmed().isEmpty()) {
        return ComponentResult(cwCoordinateText::tr("Type a coordinate, for example \"%1\".")
                                   .arg(exampleFor(order)));
    }

    const auto numbers = scanNumbers(text, order);
    if (numbers.hasError()) {
        return ComponentResult(numbers.errorMessage());
    }

    const QList<Component> components = groupNumbers(numbers.value());
    if (components.size() < kMinComponents) {
        return ComponentResult(order == cwCoordinateText::LatitudeLongitude
                                   ? cwCoordinateText::tr("A coordinate needs a latitude and a "
                                                          "longitude, for example \"%1\".")
                                         .arg(exampleFor(order))
                                   : cwCoordinateText::tr("A coordinate needs an easting and a "
                                                          "northing, for example \"%1\".")
                                         .arg(exampleFor(order)));
    }
    if (components.size() > kMaxComponents) {
        return ComponentResult(cwCoordinateText::tr("A coordinate takes at most three numbers "
                                                    "(easting, northing, elevation) — this has %1.")
                                   .arg(components.size()));
    }

    return ComponentResult(components);
}

//! What \a components amount to under \a order, or the reason they can't mean a
//! coordinate. \a units resolves a bare elevation and nothing else.
Monad::Result<cwCoordinateText::Coordinate> readCoordinate(const QList<Component>& components,
                                                           cwUnits::UnitSystem units,
                                                           cwCoordinateText::AxisOrder order)
{
    using ParseResult = Monad::Result<cwCoordinateText::Coordinate>;

    //Only the last of three components is an elevation; with two, both are
    //horizontal and neither may carry a unit. "Horizontal" is either
    //easting/northing or latitude/longitude — which is why order matters.
    const bool hasElevation = (components.size() == kMaxComponents);
    const qsizetype horizontalCount = hasElevation ? components.size() - 1 : components.size();
    for (qsizetype i = 0; i < horizontalCount; i++) {
        const QString& unit = components.at(i).unit;
        if (!unit.isEmpty()) {
            return ParseResult(order == cwCoordinateText::LatitudeLongitude
                                   ? cwCoordinateText::tr("Only the elevation can carry a unit — "
                                                          "the latitude and longitude are degrees. "
                                                          "Remove \"%1\".").arg(unit)
                                   : cwCoordinateText::tr("Only the elevation can carry a unit — "
                                                          "the easting and northing are already in "
                                                          "their coordinate system's own units. "
                                                          "Remove \"%1\".").arg(unit));
        }
    }

    cwCoordinateText::Coordinate coordinate;
    if (order == cwCoordinateText::LatitudeLongitude) {
        coordinate.northing = components.at(0).value;
        coordinate.easting = components.at(1).value;
    } else {
        coordinate.easting = components.at(0).value;
        coordinate.northing = components.at(1).value;
    }

    coordinate.hasElevation = hasElevation;
    if (hasElevation) {
        const Component& elevation = components.at(kMaxComponents - 1);
        const QString& elevationUnit = elevation.unit;
        coordinate.hasElevationUnit = !elevationUnit.isEmpty();
        const cwUnits::LengthUnit unit = elevationUnit.isEmpty()
                                             ? cwCoordinateText::elevationUnit(units)
                                             : cwUnits::toLengthUnit(elevationUnit);
        if (unit == cwUnits::LengthUnitless) {
            return ParseResult(cwCoordinateText::tr("\"%1\" isn't a length unit — try m, ft, yd, "
                                                    "in, mm, cm, km or mi.").arg(elevationUnit));
        }
        coordinate.elevation = cwUnits::convert(elevation.value, unit, cwUnits::Meters);
    }

    return ParseResult(coordinate);
}

}

cwCoordinateText::AxisOrder cwCoordinateText::axisOrderFor(const QString& cs)
{
    return cwCoordinateTransform::isGeographic(cs) ? LatitudeLongitude : EastingNorthing;
}

Monad::Result<cwCoordinateText::Coordinate>
cwCoordinateText::parse(const QString& text, cwUnits::UnitSystem units, AxisOrder order)
{
    const auto components = readComponents(text, order);
    if (components.hasError()) {
        return Monad::Result<Coordinate>(components.errorMessage());
    }
    return readCoordinate(components.value(), units, order);
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

QString cwCoordinateText::swapHorizontal(const QString& text)
{
    //Text that isn't a coordinate has no first two components to exchange, and
    //counting numbers is not the same question: "N 46 07 16 W 115 35 56" holds
    //six of them and is not a coordinate at all. The verdict is independent of
    //both the unit system and the axis order — they decide what the numbers
    //mean, never whether they can be read (see cwCoordinateTextValidator) — so
    //these two stand in for whatever the caller's row uses.
    //Driving parse()'s two stages here rather than calling it, so the spans that
    //move are the ones the verdict was reached through.
    const auto read = readComponents(text, EastingNorthing);
    if (read.hasError()) {
        return QString();
    }

    const QList<Component> components = read.value();
    if (readCoordinate(components, cwUnits::Metric, EastingNorthing).hasError()) {
        return QString();
    }

    //There are at least two components: readComponents() insists on it. A
    //component's span stops at its last number, and readCoordinate() above
    //refuses a unit on a horizontal one, so for the two being exchanged there is
    //nothing written after them anyway.
    const Component& first = components.at(0);
    const Component& second = components.at(1);
    const QString firstText = text.mid(first.start, first.length());
    const QString secondText = text.mid(second.start, second.length());

    //The later span first: replacing the earlier one can change the string's
    //length and would leave the second offset pointing somewhere else.
    QString swapped = text;
    swapped.replace(second.start, second.length(), firstText);
    swapped.replace(first.start, first.length(), secondText);
    return swapped;
}

QString cwCoordinateText::textToStore(const QString& text,
                                      const Coordinate& coordinate,
                                      cwUnits::UnitSystem units)
{
    const QString trimmed = text.trimmed();
    if (!coordinate.hasElevation || coordinate.hasElevationUnit) {
        return trimmed;
    }

    //Separators are allowed to trail the last number, so the unit can't simply
    //be appended: "46.1, -115.6, 304," would become "…304,m", which parse()
    //then refuses. The result of this function is what gets stored, and the
    //stored string is what the components are read back out of, so a form that
    //doesn't parse would zero the fix it came from.
    qsizetype end = trimmed.size();
    while (end > 0 && (trimmed.at(end - 1).isSpace() || trimmed.at(end - 1) == u',')) {
        end--;
    }
    return trimmed.left(end) + cwUnits::unitName(elevationUnit(units));
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
