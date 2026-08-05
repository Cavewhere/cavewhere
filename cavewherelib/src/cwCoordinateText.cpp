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
#include <utility>

namespace {

//! One number and whatever is written on it: a hemisphere letter in front, an
//! angle marker behind, a word behind that.
//!
//! Each of the three is optional as a whole rather than matching empty: a plain
//! `\s*[A-Za-z]*` tail would eat the space in "46.12 -115.59" and leave the two
//! numbers looking adjacent, which the separator check would then reject.
//!
//! Two apostrophes are tried before one, or "16''" would read as minutes with a
//! stray apostrophe after. "d" and "deg" are survex's and PROJ's spelling of
//! degrees and stop at a word boundary, so no unit word can be read as one.
//! Which marker a match is, and whether a word is a unit or a hemisphere, the
//! readers below decide.
const QRegularExpression& numberExpression()
{
    static const QRegularExpression expression(QStringLiteral(
        R"rx((?:(?<lead>[NSEWnsew])\s*)?)rx"
        R"rx((?<number>[+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?))rx"
        R"rx((?:\s*(?<marker>''|[dD](?:[eE][gG])?(?![A-Za-z])|[°º'′"″:]))?)rx"
        R"rx((?:\s*(?<word>[A-Za-z]+))?)rx"));
    return expression;
}

//! The expression's groups, in the order it writes them. Naming them in the
//! pattern is what documents it; reading them back by number is what keeps this
//! cheap — Qt caches no name lookups, and each one scans PCRE2's name table.
constexpr int kLeadGroup = 1;
constexpr int kNumberGroup = 2;
constexpr int kMarkerGroup = 3;
constexpr int kWordGroup = 4;

//! What may sit between two components — commas and whitespace, nothing else.
bool isSeparator(QStringView text)
{
    return std::all_of(text.cbegin(), text.cend(), [](QChar character) {
        return character.isSpace() || character == u',';
    });
}

constexpr qsizetype kMinComponents = 2;
constexpr qsizetype kMaxComponents = 3;
//! Degrees, minutes and seconds. An angle has no fourth thing to say.
constexpr qsizetype kMaxAngleParts = 3;
constexpr double kMinutesPerDegree = 60.0;
constexpr double kSecondsPerMinute = 60.0;
constexpr double kSecondsPerDegree = kMinutesPerDegree * kSecondsPerMinute;

//! What an angle marker says the number in front of it is.
enum class Marker {
    None,
    Degrees,
    Minutes,
    Seconds,
    //! A colon, which names no part of an angle: it says only that another
    //! number follows, so 46:07:16 spells out with one character what
    //! 46°07'16" spells out with three.
    Infix
};

//! Which axis a hemisphere letter says a component is.
enum class Axis { Unstated, Latitude, Longitude };

Marker markerFor(QStringView marker)
{
    if (marker.isEmpty()) {
        return Marker::None;
    }

    switch (marker.at(0).unicode()) {
    case u'°':
    case u'º': //The masculine ordinal, a common substitution for the degree sign
    case u'd':
    case u'D':
        return Marker::Degrees;
    case u'′':
        return Marker::Minutes;
    case u'\'':
        return marker.size() > 1 ? Marker::Seconds : Marker::Minutes;
    case u'"':
    case u'″':
        return Marker::Seconds;
    case u':':
        return Marker::Infix;
    }
    return Marker::None;
}

//! Whether \a marker says the number it is written on belongs at \a position of
//! the group being read, rather than starting a group of its own. Degrees,
//! minutes and seconds are written in that order and only that order, so a
//! marker naming one of them names the position it may occupy — which is what
//! lets 46°07'16" 115°35'56" split into two angles with no comma between them.
bool markerFits(Marker marker, qsizetype position)
{
    switch (marker) {
    case Marker::None:
        return true;
    case Marker::Degrees:
        return position == 0;
    case Marker::Minutes:
        return position == 1;
    case Marker::Seconds:
        return position == 2;
    case Marker::Infix:
        //Another number follows, and nothing follows the seconds.
        return position < kMaxAngleParts - 1;
    }
    return false;
}

//! Everything a hemisphere letter says about the angle it is written on: which
//! axis that angle is, and whether it is the negative half of it. Both answers
//! come from one letter, so they come from one lookup.
struct Hemisphere
{
    Axis axis = Axis::Unstated;
    bool negative = false;
};

//! What \a letter says, or an unstated axis for a letter that says nothing —
//! including the null QChar a group with no letter carries.
Hemisphere hemisphereFor(QChar letter)
{
    switch (letter.toUpper().unicode()) {
    case u'N':
        return {Axis::Latitude, false};
    case u'S':
        return {Axis::Latitude, true};
    case u'E':
        return {Axis::Longitude, false};
    case u'W':
        return {Axis::Longitude, true};
    }
    return {};
}

//! Whether \a word is a hemisphere letter rather than a unit. No length unit is
//! spelled with a single N, S, E or W, so the two can't collide.
bool isHemisphere(QStringView word)
{
    return word.size() == 1 && hemisphereFor(word.at(0)).axis != Axis::Unstated;
}

QString unreadableText(QStringView word)
{
    return cwCoordinateText::tr("Couldn't read \"%1\" as part of a coordinate.")
        .arg(word.trimmed().toString());
}

//! Whether a row under \a order can hold an angle at all, which is the one thing
//! the order settles about what the text *means*: degrees, minutes and seconds
//! are a latitude and a longitude, and so is a hemisphere letter. Named because
//! the order is also read for questions it answers differently — which axis
//! leads, what the two axes are called — and those comparisons are not this one.
bool readsAngles(cwCoordinateText::AxisOrder order)
{
    return order == cwCoordinateText::LatitudeLongitude;
}

//! An example in the order being read, so the message a user gets back
//! demonstrates the arrangement their own row wants rather than the other one.
//!
//! Decimal degrees, always: the angle examples below appear only in the messages
//! where writing an angle is what the user was doing.
QString exampleFor(cwCoordinateText::AxisOrder order)
{
    return order == cwCoordinateText::LatitudeLongitude
               ? QStringLiteral("46.12113, -115.59902, 304m")
               : QStringLiteral("610016.792, 5615117.075, 2545.34m");
}

//! The same place as exampleFor()'s geographic example, written as an angle: one
//! coordinate throughout, so a user who reads two messages reads one example.
QString symbolAngleExample()
{
    return QStringLiteral("46°07'16.1\" N, 115°35'56.5\" W");
}

//! And the spelling that needs no symbols, which is also how survex and Walls
//! write it — so it is what a #FIX line pasted out of a project file looks like.
QString colonAngleExample()
{
    return QStringLiteral("46:07:16.1, -115:35:56.5");
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

//! One number as it was written: what it says, the span it occupies, and
//! everything written around it.
//!
//! The span covers the number and its own marker, so a coordinate rewritten by
//! moving spans moves 46°07'16" whole. A hemisphere letter and a word sit
//! outside it: they say what the number *is*, and swapHorizontal() leaves both
//! where they were written.
struct Number
{
    double value = 0.0;
    qsizetype start = 0;
    qsizetype end = 0;
    Marker marker = Marker::None;
    //! The word written after this number: a unit, or a hemisphere letter that
    //! splitGroups() hasn't claimed for its group. On a row that reads no
    //! hemispheres it also holds a letter written in *front* of the number, which
    //! splitGroups() moves here so one rule answers both sides — so which side it
    //! was written on is a question only the scan can still answer.
    QString word;
    //! The hemisphere letter written in front of this number, if any.
    QChar lead;
    //! Whether a sign was written at all. Which sign it was, \a value already
    //! carries — but a written "+" and nothing at all are the same number, and
    //! only the degrees of an angle may carry either.
    bool hasSign = false;
    bool commaBefore = false;
};

//! Whether any of \a numbers carries an angle marker — the one piece of evidence
//! that says "angle" wherever it is written, whatever the row it is written on.
bool hasMarker(const QList<Number>& numbers)
{
    return std::any_of(numbers.cbegin(), numbers.cend(), [](const Number& number) {
        return number.marker != Marker::None;
    });
}

//! One component's numbers, and the hemisphere letter that claims them.
struct NumberGroup
{
    QList<Number> parts;
    QChar letter;
    //! Whether something written after the last number ended the group. A
    //! hemisphere letter and a unit both belong to what they follow, so the
    //! next number starts a group of its own.
    bool closed = false;
};

//! One component of a coordinate: what it says, the word written after it, the
//! span of text it occupies, and how it was written.
//!
//! groupNumbers() is the one place that decides how numbers combine into a
//! component, so a component arrives already knowing its own value rather than
//! leaving each reader to derive one from parts.
struct Component
{
    double value = 0.0;
    QString unit;
    qsizetype start = 0;
    qsizetype end = 0;
    //! Which axis a hemisphere letter said this is, wherever in the coordinate
    //! it was written. Unstated leaves the axis to the row's own order.
    Axis axis = Axis::Unstated;
    //! Whether it was written as an angle rather than as a plain number — an
    //! angle marker on any of its numbers, or a hemisphere letter on the group.
    bool isAngle = false;

    qsizetype length() const { return end - start; }
};

//! Every number \a text spells out, each with what was written around it, or the
//! reason something between two of them isn't a thing a coordinate may contain.
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

    QList<Number> numbers;
    qsizetype cursor = 0;

    QRegularExpressionMatchIterator iterator = numberExpression().globalMatch(text);
    while (iterator.hasNext()) {
        const QRegularExpressionMatch match = iterator.next();
        const QStringView gap = QStringView(text).mid(cursor, match.capturedStart() - cursor);

        if (!isSeparator(gap)) {
            return ScanResult(unreadableText(gap));
        }
        //Numbers have to be separated by something. "46.12-115.6" reads as two
        //numbers to the expression above, but it is a typo far more often than
        //it is a coordinate. A marker or a word written between them is that
        //something: 46°07 and 46:07 have nothing else to go on.
        if (!numbers.isEmpty() && gap.isEmpty()
            && numbers.constLast().marker == Marker::None
            && numbers.constLast().word.isEmpty()) {
            //The example follows the notation the text is already writing:
            //"46:07:16.1-115:35:56.5" is one comma short of reading, and a
            //decimal example there answers a question nobody asked. Asked on the
            //way out only, so the scan itself carries no state for it.
            const bool writingAnAngle = readsAngles(order) && hasMarker(numbers);
            return ScanResult(cwCoordinateText::tr("Separate the numbers with commas or spaces, "
                                                   "for example \"%1\".")
                                  .arg(writingAnAngle ? colonAngleExample() : exampleFor(order)));
        }

        const QStringView digits = match.capturedView(kNumberGroup);
        bool ok = false;
        const double value = digits.toDouble(&ok);
        if (!ok || !std::isfinite(value)) {
            return ScanResult(cwCoordinateText::tr("\"%1\" is too big to be a coordinate.")
                                  .arg(digits));
        }

        //An unset group captures an empty view, which markerFor() reads as no
        //marker; the end offset is the one answer that needs asking for, since
        //capturedEnd() on an unset group is -1.
        const bool markerCaptured = match.hasCaptured(kMarkerGroup);
        const QChar sign = digits.at(0);

        Number number;
        number.value = value;
        number.start = match.capturedStart(kNumberGroup);
        number.end = markerCaptured ? match.capturedEnd(kMarkerGroup)
                                    : match.capturedEnd(kNumberGroup);
        number.marker = markerFor(match.capturedView(kMarkerGroup));
        number.word = match.captured(kWordGroup);
        number.lead = match.hasCaptured(kLeadGroup) ? match.capturedView(kLeadGroup).at(0) : QChar();
        number.hasSign = (sign == u'+' || sign == u'-');
        number.commaBefore = gap.contains(u',');

        numbers.append(number);
        cursor = match.capturedEnd();
    }

    const QStringView tail = QStringView(text).mid(cursor);
    if (!isSeparator(tail)) {
        return ScanResult(unreadableText(tail));
    }

    return ScanResult(std::move(numbers));
}

//! \a numbers split into the groups the text writes them in, or the reason a
//! hemisphere letter has no number it can belong to.
//!
//! A group ends at a comma, at a hemisphere letter or a unit written after its
//! last number, at a hemisphere letter written in front of the next one, at a
//! marker that doesn't fit the position it would take, and at three numbers.
//! Nothing here decides whether a group *is* an angle — isAngleGroup() does.
//!
//! \a readsHemispheres says whether a letter is a hemisphere at all, and it is
//! the one ruling this layer makes about what text means rather than how it is
//! shaped. A hemisphere is a geographic idea, and on a projected row "500000 E"
//! is far more likely to be labeling an easting, so there a letter stays the unit
//! it has always been — one a horizontal component may not carry, on whichever
//! side of its number it was written. Made here rather than left to
//! readCoordinate() because it decides whether numbers *combine*, and a UTM paste
//! read as one angle is answered by an angle's rules instead of its own.
Monad::Result<QList<NumberGroup>> splitGroups(const QList<Number>& numbers,
                                              bool readsHemispheres)
{
    using SplitResult = Monad::Result<QList<NumberGroup>>;

    QList<NumberGroup> groups;
    NumberGroup current;
    //A letter that has been read but belongs to the group about to start.
    QChar pending;

    for (const Number& number : numbers) {
        const bool boundary = current.closed
                              || number.commaBefore
                              || !number.lead.isNull()
                              || current.parts.size() >= kMaxAngleParts
                              || !markerFits(number.marker, current.parts.size());
        if (!current.parts.isEmpty() && boundary) {
            groups.append(current);
            current = NumberGroup();
        }

        Number part = number;

        if (readsHemispheres) {
            if (!part.lead.isNull()) {
                if (!pending.isNull()) {
                    return SplitResult(unreadableText(QStringView(&pending, 1)));
                }
                current.letter = part.lead;
            } else if (!pending.isNull()) {
                current.letter = pending;
                pending = QChar();
            }
        } else if (!part.lead.isNull() && part.word.isEmpty()) {
            //A letter that isn't a hemisphere is a unit from either side of its
            //number, so both sides get the one rule that refuses it. A word
            //already written here is the more specific answer and keeps its place.
            part.word = part.lead;
        }

        if (readsHemispheres && isHemisphere(part.word)) {
            //The evidence for a group can arrive after its numbers: nothing in
            //"46 07.268 N" joins the two until the letter does. A second letter
            //can't also be this group's, so it opens the next one.
            const QChar letter = part.word.at(0);
            part.word.clear();
            if (current.letter.isNull()) {
                current.letter = letter;
            } else {
                pending = letter;
            }
            current.closed = true;
        } else if (!part.word.isEmpty()) {
            current.closed = true;
        }
        current.parts.append(part);
    }

    if (!current.parts.isEmpty()) {
        groups.append(current);
    }
    if (!pending.isNull()) {
        return SplitResult(unreadableText(QStringView(&pending, 1)));
    }

    return SplitResult(std::move(groups));
}

//! Whether \a group was written as an angle rather than as plain numbers.
//!
//! The property the whole of #654 rests on: a group says it is an angle only
//! through a marker or a hemisphere letter, so text carrying neither reads
//! exactly as it always has. "46 07 16" is a latitude, a longitude and an
//! elevation, not 46°07'16".
bool isAngleGroup(const NumberGroup& group)
{
    return !group.letter.isNull() || hasMarker(group.parts);
}

//! The angle \a group spells out, or the reason it isn't one. Its numbers are
//! the degrees, then the minutes, then the seconds, whichever of the three were
//! written, negated by the degrees' own sign or by a southern or western
//! hemisphere.
//!
//! How far the angle reaches is deliberately not checked: a latitude past 90°
//! or a longitude past 180° already has a reporter in the coordinate system's
//! own domain diagnostic, and two voices on one complaint is worse than one.
Monad::Result<Component> readAngle(const NumberGroup& group)
{
    using AngleResult = Monad::Result<Component>;

    const QList<Number>& parts = group.parts;
    const Number& degrees = parts.constFirst();
    const Hemisphere hemisphere = hemisphereFor(group.letter);

    //A colon is the one marker that says something about what comes next rather
    //than about the number it is written on, so a group can't end on one. This
    //catches both the half-typed "46:07:16:" and "46:07:16:22", where the fourth
    //number is one too many for an angle to hold.
    if (parts.constLast().marker == Marker::Infix) {
        return AngleResult(cwCoordinateText::tr("A colon says another number follows it."));
    }

    //Only the first number can carry a marker that doesn't fit: splitGroups()
    //starts a new group at any later one, so a misfit here is a minutes or
    //seconds mark with no degrees in front of it — which is also what 1000' as
    //an elevation is.
    if (!markerFits(degrees.marker, 0)) {
        return AngleResult(cwCoordinateText::tr("Minutes and seconds need degrees written in "
                                                "front of them."));
    }

    for (qsizetype i = 0; i < parts.size(); i++) {
        const Number& part = parts.at(i);
        if (i > 0 && part.hasSign) {
            return AngleResult(cwCoordinateText::tr("Only the degrees of an angle carry a sign, "
                                                    "and it negates the whole angle."));
        }
        //Two sentences rather than one: each names the place that overflowed, and
        //that is what says how to rewrite it.
        if (i == 1 && part.value >= kMinutesPerDegree) {
            return AngleResult(cwCoordinateText::tr("An angle's minutes are less than 60 — \"%1\" is "
                                                    "a whole degree or more.")
                                   .arg(shortestNumber(part.value)));
        }
        if (i == 2 && part.value >= kSecondsPerMinute) {
            return AngleResult(cwCoordinateText::tr("An angle's seconds are less than 60 — \"%1\" is "
                                                    "a whole minute or more.")
                                   .arg(shortestNumber(part.value)));
        }
        if (i + 1 < parts.size() && part.value != std::trunc(part.value)) {
            return AngleResult(cwCoordinateText::tr("Only the last number of an angle can have a "
                                                    "decimal fraction."));
        }
    }

    if (degrees.hasSign && !group.letter.isNull()) {
        return AngleResult(cwCoordinateText::tr("An angle carries a sign or a hemisphere, not "
                                                "both — remove the sign or the \"%1\".")
                               .arg(group.letter));
    }

    //Built from the magnitude and signed once at the end, so a negative angle
    //reads as the whole of it rather than as degrees minus minutes.
    double magnitude = std::abs(degrees.value);
    if (parts.size() > 1) {
        magnitude += parts.at(1).value / kMinutesPerDegree;
    }
    if (parts.size() > 2) {
        magnitude += parts.at(2).value / kSecondsPerDegree;
    }
    const bool negative = std::signbit(degrees.value) || hemisphere.negative;

    Component component;
    component.value = negative ? -magnitude : magnitude;
    component.unit = parts.constLast().word;
    component.start = degrees.start;
    component.end = parts.constLast().end;
    component.axis = hemisphere.axis;
    component.isAngle = true;
    return AngleResult(component);
}

//! \a numbers assembled into the components they spell out, or the reason they
//! can't spell out any.
//!
//! The one place that decides how numbers combine, and combining any takes
//! evidence — see isAngleGroup(). \a order decides only whether a letter is a
//! hemisphere; everything else here is read from the text alone.
Monad::Result<QList<Component>> groupNumbers(const QList<Number>& numbers,
                                             cwCoordinateText::AxisOrder order)
{
    using ComponentResult = Monad::Result<QList<Component>>;

    const auto split = splitGroups(numbers, readsAngles(order));
    if (split.hasError()) {
        return ComponentResult(split.errorMessage());
    }

    QList<Component> components;
    const QList<NumberGroup> groups = split.value();
    for (const NumberGroup& group : groups) {
        if (!isAngleGroup(group)) {
            //Nothing joins these, so each number is a component of its own.
            for (const Number& part : group.parts) {
                components.append(Component{.value = part.value,
                                            .unit = part.word,
                                            .start = part.start,
                                            .end = part.end});
            }
            continue;
        }

        const auto angle = readAngle(group);
        if (angle.hasError()) {
            return ComponentResult(angle.errorMessage());
        }
        components.append(angle.value());
    }

    return ComponentResult(std::move(components));
}

//! The components \a text spells out, or the reason it isn't a coordinate.
//!
//! Nearly everything decided here is decided from the text alone: what the
//! components mean — which axis each one is, what unit a bare elevation carries,
//! whether the row may hold an angle at all — is readCoordinate()'s question.
//! \a order phrases the messages, and settles the one textual question that
//! isn't self-contained: whether a letter is a hemisphere (see splitGroups()).
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

    const auto grouped = groupNumbers(numbers.value(), order);
    if (grouped.hasError()) {
        return ComponentResult(grouped.errorMessage());
    }

    const QList<Component> components = grouped.value();
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
        //The message #654 opens with lands here: "46 07 16.1, -115 35 56.5" is
        //six numbers, and the count alone teaches nothing about the one thing
        //missing from it. The hint is geographic-only because on any other row
        //marking them would be wrong advice — an angle is refused there outright.
        return ComponentResult(readsAngles(order)
                                   ? cwCoordinateText::tr("A coordinate takes at most three numbers "
                                                          "(latitude, longitude, elevation) — this "
                                                          "has %1. Mark degrees, minutes and seconds "
                                                          "to read them as one angle: \"%2\" or "
                                                          "\"%3\".")
                                         .arg(QString::number(components.size()),
                                              symbolAngleExample(),
                                              colonAngleExample())
                                   : cwCoordinateText::tr("A coordinate takes at most three numbers "
                                                          "(easting, northing, elevation) — this "
                                                          "has %1.")
                                         .arg(components.size()));
    }

    return ComponentResult(components);
}

//! What \a components amount to under \a order, or the reason they can't mean a
//! coordinate. \a units resolves a bare elevation and nothing else.
//!
//! \a order answers three questions here: whether the row can hold an angle at
//! all, which axis a component with no hemisphere letter on it is, and how the
//! messages are worded.
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

    //Degrees, minutes and seconds are a latitude and a longitude, so a row that
    //isn't geographic has nowhere to put one. The far likelier cause is the
    //wrong coordinate system than a wish for degrees in an easting. One wording
    //serves two rows — a projected one, and one with no system at all, which
    //axisOrderFor() collapses onto the same order — because parse() is handed an
    //axis order and never the coordinate system itself.
    if (!readsAngles(order)
        && std::any_of(components.cbegin(), components.cend(), [](const Component& component) {
               return component.isAngle;
           })) {
        return ParseResult(cwCoordinateText::tr("Degrees, minutes and seconds are a latitude and "
                                                "longitude. Set this station's coordinate system "
                                                "to a geographic one, or write the coordinate in "
                                                "the units its own system uses."));
    }

    if (hasElevation && components.constLast().isAngle) {
        return ParseResult(cwCoordinateText::tr("Only a latitude and a longitude are angles — the "
                                                "elevation is a height, in meters or feet."));
    }

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

    //A hemisphere letter is authoritative: N or S makes its component a
    //latitude and E or W a longitude, wherever in the coordinate it was
    //written, so "115°35'W, 46°07'N" reads the way it was meant rather than the
    //way its arrangement suggests. This is still never inferring the order from
    //the numbers — the letter is the user stating it — and without one the row's
    //own order decides, as it always has.
    const Component& first = components.at(0);
    const Component& second = components.at(1);
    if (first.axis != Axis::Unstated && first.axis == second.axis) {
        return ParseResult(first.axis == Axis::Latitude
                               ? cwCoordinateText::tr("A coordinate needs a latitude and a "
                                                      "longitude — both of these say latitude. "
                                                      "Mark one of them E or W.")
                               : cwCoordinateText::tr("A coordinate needs a latitude and a "
                                                      "longitude — both of these say longitude. "
                                                      "Mark one of them N or S."));
    }

    const bool firstIsNorthing = [&]() {
        if (first.axis != Axis::Unstated) {
            return first.axis == Axis::Latitude;
        }
        if (second.axis != Axis::Unstated) {
            return second.axis == Axis::Longitude;
        }
        return order == cwCoordinateText::LatitudeLongitude;
    }();

    cwCoordinateText::Coordinate coordinate;
    if (firstIsNorthing) {
        coordinate.northing = first.value;
        coordinate.easting = second.value;
    } else {
        coordinate.easting = first.value;
        coordinate.northing = second.value;
    }

    coordinate.hasElevation = hasElevation;
    if (hasElevation) {
        const Component& elevation = components.constLast();
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
    //counting numbers is not the same question: "46.12-115.6" holds two and is
    //not a coordinate at all. The verdict is independent of the unit system —
    //that decides what a bare elevation means, never whether it can be read (see
    //cwCoordinateTextValidator) — so Metric stands in for whatever the caller's
    //row uses. The axis order it is not independent of: an angle is a latitude
    //and a longitude, so the geographic order is the only one that can read
    //everything a swap could be offered for. It is also the order the question
    //arises in, since what this exists for is a row just named geographic.
    //Driving parse()'s two stages here rather than calling it, so the spans that
    //move are the ones the verdict was reached through.
    constexpr AxisOrder probeOrder = LatitudeLongitude;

    const auto read = readComponents(text, probeOrder);
    if (read.hasError()) {
        return QString();
    }

    const QList<Component> components = read.value();
    if (readCoordinate(components, cwUnits::Metric, probeOrder).hasError()) {
        return QString();
    }

    //There are at least two components: readComponents() insists on it. A
    //component's span stops at its last number and its marker, and
    //readCoordinate() above refuses a unit on a horizontal one, so for the two
    //being exchanged there is nothing written after them anyway.
    const Component& first = components.at(0);
    const Component& second = components.at(1);

    //A hemisphere letter says which axis its own component is, whichever side of
    //the coordinate it sits on, so the order was written down after all and
    //there is nothing left to ask. CoordinateOrderAskBox gates on an empty
    //answer, so it goes quiet on lettered text by itself.
    if (first.axis != Axis::Unstated || second.axis != Axis::Unstated) {
        return QString();
    }

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
