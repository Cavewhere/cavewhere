//Our includes
#include "cwCoordinateText.h"
#include "cwUnits.h"

//Catch includes
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

//Qt includes
#include <QLocale>
#include <QString>

using Catch::Approx;

namespace {

//! Approx's default relative tolerance is ~1.19e-5, which on a UTM easting is
//! ±7 m — coarser than the precision this whole feature exists to preserve, so
//! a parser that dropped to float would satisfy it. Every coordinate comparison
//! goes through this instead.
Approx tight(double value)
{
    return Approx(value).epsilon(1e-12);
}

//! The parsed coordinate, or a Catch failure naming the reason it wouldn't parse.
cwCoordinateText::Coordinate parsed(const QString& text,
                                    cwUnits::UnitSystem units = cwUnits::Metric,
                                    cwCoordinateText::AxisOrder order = cwCoordinateText::EastingNorthing)
{
    const auto result = cwCoordinateText::parse(text, units, order);
    INFO("parsing \"" << text.toStdString() << "\": "
                      << result.errorMessage().toStdString());
    REQUIRE_FALSE(result.hasError());
    return result.value();
}

//! The reason \a text wouldn't parse, or a Catch failure if it parsed.
QString rejection(const QString& text, cwUnits::UnitSystem units = cwUnits::Metric)
{
    const auto result = cwCoordinateText::parse(text, units, cwCoordinateText::EastingNorthing);
    INFO("expected \"" << text.toStdString() << "\" to be rejected");
    REQUIRE(result.hasError());
    //An empty reason would leave both entry surfaces showing a blank error.
    CHECK_FALSE(result.errorMessage().isEmpty());
    return result.errorMessage();
}

constexpr double kFeetToMeters = 0.3048;

}

TEST_CASE("cwCoordinateText reads the formats #621 asks for", "[FixStation][cwCoordinateText]") {
    //#621's lat/long examples are written latitude first — 46.12113 is a
    //latitude in Idaho and -115.59902 the longitude — so they parse under
    //LatitudeLongitude and land in northing / easting respectively.
    const auto latLong = cwCoordinateText::LatitudeLongitude;

    SECTION("lat/long with meters") {
        const auto coordinate = parsed("46.12113, -115.59902, 304m", cwUnits::Metric, latLong);
        CHECK(coordinate.northing == tight(46.12113));
        CHECK(coordinate.easting == tight(-115.59902));
        CHECK(coordinate.elevation == tight(304.0));
    }

    SECTION("lat/long with feet converts to meters") {
        const auto coordinate = parsed("46.12113, -115.59902, 304ft", cwUnits::Metric, latLong);
        CHECK(coordinate.northing == tight(46.12113));
        CHECK(coordinate.easting == tight(-115.59902));
        CHECK(coordinate.elevation == tight(304.0 * kFeetToMeters));
    }

    SECTION("UTM with meters") {
        const auto coordinate = parsed("610016.792, 5615117.075, 2545.340m");
        CHECK(coordinate.easting == tight(610016.792));
        CHECK(coordinate.northing == tight(5615117.075));
        CHECK(coordinate.elevation == tight(2545.340));
    }

    SECTION("UTM with a spelled-out unit and a space") {
        const auto coordinate = parsed("610016.792, 5615117.075, 2545.340 feet");
        CHECK(coordinate.easting == tight(610016.792));
        CHECK(coordinate.elevation == tight(2545.340 * kFeetToMeters));
    }
}

TEST_CASE("cwCoordinateText writes latitude first for a geographic CS",
          "[FixStation][cwCoordinateText]") {
    //The two orders cannot be told apart from the numbers — 46.12113,
    //-115.59902 is a legal UTM pair — so this is the whole of the rule.
    SECTION("the order comes from the coordinate system") {
        CHECK(cwCoordinateText::axisOrderFor(QStringLiteral("EPSG:4326"))
              == cwCoordinateText::LatitudeLongitude);
        CHECK(cwCoordinateText::axisOrderFor(QStringLiteral("EPSG:32613"))
              == cwCoordinateText::EastingNorthing);
    }

    SECTION("an absent or unusable CS falls back to easting first") {
        //A fix that declares no CS defers to the region's global one, which
        //survex requires to be projected — so there is no geographic case
        //hiding behind the empty string.
        CHECK(cwCoordinateText::axisOrderFor(QString()) == cwCoordinateText::EastingNorthing);
        CHECK(cwCoordinateText::axisOrderFor(QStringLiteral("  "))
              == cwCoordinateText::EastingNorthing);
        CHECK(cwCoordinateText::axisOrderFor(QStringLiteral("not a CS"))
              == cwCoordinateText::EastingNorthing);
    }

    SECTION("the same text means opposite things under the two orders") {
        const auto asLatLong = parsed("46.12113, -115.59902, 0m", cwUnits::Metric,
                                      cwCoordinateText::LatitudeLongitude);
        const auto asProjected = parsed("46.12113, -115.59902, 0m", cwUnits::Metric,
                                        cwCoordinateText::EastingNorthing);

        CHECK(asLatLong.northing == tight(asProjected.easting));
        CHECK(asLatLong.easting == tight(asProjected.northing));
    }

    SECTION("format writes the latitude first too") {
        //Feeding format() a different order than parse() would transpose the
        //coordinate on every trip through the field, which is the failure this
        //whole parameter exists to prevent.
        CHECK(cwCoordinateText::format(-115.59902, 46.12113, 304.0, cwUnits::Metric,
                                       cwCoordinateText::LatitudeLongitude)
              == QStringLiteral("46.12113, -115.59902, 304m"));
        CHECK(cwCoordinateText::format(-115.59902, 46.12113, 304.0, cwUnits::Metric,
                                       cwCoordinateText::EastingNorthing)
              == QStringLiteral("-115.59902, 46.12113, 304m"));
    }
}

TEST_CASE("cwCoordinateText reads a bare elevation in the project's units",
          "[FixStation][cwCoordinateText]") {
    SECTION("metric leaves it alone") {
        CHECK(parsed("46.12113, -115.59902, 304", cwUnits::Metric).elevation == tight(304.0));
    }

    SECTION("imperial reads it as feet") {
        CHECK(parsed("46.12113, -115.59902, 304", cwUnits::Imperial).elevation
              == tight(304.0 * kFeetToMeters));
    }

    SECTION("an explicit unit wins over the project's") {
        CHECK(parsed("46.12113, -115.59902, 304m", cwUnits::Imperial).elevation == tight(304.0));
        CHECK(parsed("46.12113, -115.59902, 304ft", cwUnits::Metric).elevation
              == tight(304.0 * kFeetToMeters));
    }

    SECTION("and says which of the two it was") {
        //Whether the unit was written or inferred is the difference between a
        //string that keeps its meaning and one that changes it when the project
        //flips units — see textToStore().
        CHECK_FALSE(parsed("46.12113, -115.59902, 304").hasElevationUnit);
        CHECK(parsed("46.12113, -115.59902, 304m").hasElevationUnit);
        CHECK(parsed("46.12113, -115.59902, 304 ft").hasElevationUnit);
        CHECK_FALSE(parsed("46.12113, -115.59902").hasElevationUnit);
    }
}

TEST_CASE("cwCoordinateText spells out the unit a bare elevation was read in",
          "[FixStation][cwCoordinateText]") {
    //U14's Trap 1. "304" means "304 in the project's units" at the moment it is
    //typed; stored verbatim it silently becomes 304 ft once the project turns
    //imperial. Appending the unit that was actually resolved is the only edit
    //this feature makes to the user's own words.
    const auto stored = [](const QString& text, cwUnits::UnitSystem units) {
        return cwCoordinateText::textToStore(text, parsed(text, units), units);
    };

    SECTION("a bare elevation gets the project's unit") {
        CHECK(stored("46.12113, -115.59902, 304", cwUnits::Metric)
              == QStringLiteral("46.12113, -115.59902, 304m"));
        CHECK(stored("46.12113, -115.59902, 304", cwUnits::Imperial)
              == QStringLiteral("46.12113, -115.59902, 304ft"));
    }

    SECTION("text that already says so is left as it was written") {
        //Including the unit the user chose rather than the project's: the point
        //is to keep their string, not to restate it.
        CHECK(stored("46.12113, -115.59902, 304ft", cwUnits::Metric)
              == QStringLiteral("46.12113, -115.59902, 304ft"));
        CHECK(stored("46.12113, -115.59902, 304 m", cwUnits::Imperial)
              == QStringLiteral("46.12113, -115.59902, 304 m"));
    }

    SECTION("a two-component paste has no elevation to describe") {
        CHECK(stored("46.12113, -115.59902", cwUnits::Imperial)
              == QStringLiteral("46.12113, -115.59902"));
    }

    SECTION("surrounding whitespace goes, so a re-commit matches what was stored") {
        CHECK(stored("  46.12113, -115.59902, 304m  ", cwUnits::Metric)
              == QStringLiteral("46.12113, -115.59902, 304m"));
        CHECK(stored("  46.12113, -115.59902, 304  ", cwUnits::Metric)
              == QStringLiteral("46.12113, -115.59902, 304m"));
    }
}

TEST_CASE("cwCoordinateText accepts the separators a paste actually arrives with",
          "[FixStation][cwCoordinateText]") {
    SECTION("spaces instead of commas") {
        const auto coordinate = parsed("46.12113 -115.59902 304m");
        CHECK(coordinate.easting == tight(46.12113));
        CHECK(coordinate.northing == tight(-115.59902));
        CHECK(coordinate.elevation == tight(304.0));
    }

    SECTION("surrounding and doubled whitespace") {
        const auto coordinate = parsed("  46.12113 ,  -115.59902 ,  304 m  ");
        CHECK(coordinate.northing == tight(-115.59902));
        CHECK(coordinate.elevation == tight(304.0));
    }

    SECTION("an explicit plus sign") {
        CHECK(parsed("+46.12113, -115.59902, +304m").easting == tight(46.12113));
    }

    SECTION("a leading decimal point") {
        CHECK(parsed(".5, -.25, 0m").easting == tight(0.5));
    }

    SECTION("scientific notation, which is what format() emits for huge values") {
        CHECK(parsed("1e6, 2.5e6, 0m").easting == tight(1e6));
    }
}

TEST_CASE("cwCoordinateText reads each number as its own component",
          "[FixStation][cwCoordinateText]") {
    //The property degrees, minutes and seconds (#654) get built on: nothing a
    //coordinate can say today joins two numbers into one component, so three
    //numbers are three components whatever separates them.
    SECTION("three numbers are two horizontals and an elevation, not one angle") {
        //"46 07 16" is a latitude of 46, a longitude of 7 and an elevation of
        //16 m. It has to keep meaning that once 46°07'16" means something else.
        const auto coordinate = parsed("46 07 16", cwUnits::Metric,
                                       cwCoordinateText::LatitudeLongitude);
        CHECK(coordinate.northing == tight(46.0));
        CHECK(coordinate.easting == tight(7.0));
        CHECK(coordinate.elevation == tight(16.0));
        CHECK(coordinate.hasElevation);
    }

    SECTION("two numbers are two horizontals, not degrees and minutes") {
        const auto coordinate = parsed("46 07.268", cwUnits::Metric,
                                       cwCoordinateText::LatitudeLongitude);
        CHECK(coordinate.northing == tight(46.0));
        CHECK(coordinate.easting == tight(7.268));
        CHECK_FALSE(coordinate.hasElevation);
    }
}

TEST_CASE("cwCoordinateText leaves the elevation at zero when none is given",
          "[FixStation][cwCoordinateText]") {
    const auto coordinate = parsed("46.12113, -115.59902");
    CHECK(coordinate.easting == tight(46.12113));
    CHECK(coordinate.northing == tight(-115.59902));
    CHECK(coordinate.elevation == tight(0.0));

    SECTION("and still puts the latitude first without one") {
        //The elevation-less form is the one a lat/long gets pasted in most
        //often, and it is the form that would survive an axis swap applied
        //only on the three-component path.
        const auto latLong = parsed("46.12113, -115.59902", cwUnits::Metric,
                                    cwCoordinateText::LatitudeLongitude);
        CHECK(latLong.northing == tight(46.12113));
        CHECK(latLong.easting == tight(-115.59902));
    }
}

TEST_CASE("cwCoordinateText reads and writes in the C locale, both directions",
          "[FixStation][cwCoordinateText]") {
    //parse() is C-locale by way of QString::toDouble; format() has to match, or
    //a German-locale user gets a field that refuses to read its own output.
    const QLocale previous = QLocale();
    QLocale::setDefault(QLocale(QLocale::German));

    const QString text = cwCoordinateText::format(46.12113, -115.59902, 304.0,
                                                  cwUnits::Metric,
                                                  cwCoordinateText::EastingNorthing);
    CHECK(text == QStringLiteral("46.12113, -115.59902, 304m"));
    CHECK(parsed(text).easting == tight(46.12113));

    QLocale::setDefault(previous);
}

TEST_CASE("cwCoordinateText rejects what it can't make sense of",
          "[FixStation][cwCoordinateText]") {
    SECTION("empty text") {
        rejection("");
        rejection("   ");
    }

    SECTION("one number is not a coordinate") {
        rejection("46.12113");
    }

    SECTION("four numbers are not a coordinate") {
        rejection("1, 2, 3, 4");
    }

    SECTION("words") {
        //The message has to quote the offending text — "invalid" alone leaves
        //the user hunting through a pasted line for what upset it. The probe
        //word deliberately appears in no message: "north" would have been
        //satisfied by the word "northing" in an unrelated one.
        CHECK(rejection("46.12113, gibberish, 304m").contains(QStringLiteral("gibberish")));
        rejection("somewhere near the entrance");
    }

    SECTION("a number too big to be a double") {
        //QString::toDouble reports overflow by failing rather than by returning
        //infinity, so without the !ok check this would silently read as 0 — a
        //fix at null island, with no error anywhere.
        rejection("1e400, 2, 3m");
        rejection("46.12113, -115.59902, 1e400m");
    }

    SECTION("trailing junk") {
        CHECK(rejection("46.12113, -115.59902, 304m (approx)")
                  .contains(QStringLiteral("approx")));
    }

    SECTION("numbers with nothing between them") {
        //"46.12-115.6" is two numbers to the tokenizer, and a typo in practice.
        rejection("46.12-115.6, 304m");
    }

    SECTION("a unit that isn't a length") {
        CHECK(rejection("46.12113, -115.59902, 304 bananas")
                  .contains(QStringLiteral("bananas")));
    }

    SECTION("a unit on a horizontal component") {
        //Easting and northing are in their CS's own units, which this parser
        //has no way to know, so a suffix there can only be wrong.
        //Probing for "m" would have passed on any message at all — every one of
        //them contains a lowercase m somewhere.
        CHECK(rejection("46.12113m, -115.59902, 304m")
                  .contains(QStringLiteral("Only the elevation")));
        rejection("610016.792ft, 5615117.075ft, 2545.34ft");
    }

    SECTION("a unit on the northing of a two-component coordinate") {
        //Nothing here is an elevation, so the second component is a northing
        //and may not carry a unit either.
        rejection("46.12113, -115.59902m");
    }
}

TEST_CASE("cwCoordinateText explains itself in the order being read",
          "[FixStation][cwCoordinateText]") {
    //A message that names the wrong axis, or shows a UTM example to someone
    //typing a latitude, is worse than no message — the whole point of the
    //free-form field is that it says what it wanted.
    const auto reasonFor = [](const QString& text, cwCoordinateText::AxisOrder order) {
        const auto result = cwCoordinateText::parse(text, cwUnits::Metric, order);
        REQUIRE(result.hasError());
        return result.errorMessage();
    };

    SECTION("too few components names the axes the row actually uses") {
        CHECK(reasonFor(QStringLiteral("46.12113"), cwCoordinateText::LatitudeLongitude)
                  .contains(QStringLiteral("latitude")));
        CHECK(reasonFor(QStringLiteral("46.12113"), cwCoordinateText::EastingNorthing)
                  .contains(QStringLiteral("easting")));
    }

    SECTION("a unit on a horizontal names the axes the row actually uses") {
        CHECK(reasonFor(QStringLiteral("46.12113m, -115.59902, 304m"),
                        cwCoordinateText::LatitudeLongitude)
                  .contains(QStringLiteral("latitude")));
        CHECK(reasonFor(QStringLiteral("46.12113m, -115.59902, 304m"),
                        cwCoordinateText::EastingNorthing)
                  .contains(QStringLiteral("easting")));
    }

    SECTION("the worked example matches the order") {
        CHECK(reasonFor(QString(), cwCoordinateText::LatitudeLongitude)
                  .contains(QStringLiteral("46.12113, -115.59902, 304m")));
        CHECK(reasonFor(QString(), cwCoordinateText::EastingNorthing)
                  .contains(QStringLiteral("610016.792, 5615117.075, 2545.34m")));
    }
}

TEST_CASE("cwCoordinateText formats what it parses", "[FixStation][cwCoordinateText]") {
    SECTION("metric writes meters") {
        CHECK(cwCoordinateText::format(46.12113, -115.59902, 304.0, cwUnits::Metric,
                                       cwCoordinateText::EastingNorthing)
              == QStringLiteral("46.12113, -115.59902, 304m"));
    }

    SECTION("imperial writes feet") {
        CHECK(cwCoordinateText::format(46.12113, -115.59902, 304.0 * kFeetToMeters,
                                       cwUnits::Imperial, cwCoordinateText::EastingNorthing)
              == QStringLiteral("46.12113, -115.59902, 304ft"));
    }

    SECTION("round trip keeps the horizontals bit-for-bit and the elevation to the ulp") {
        //The single field is both the display and the input, so a user who opens
        //the editor and presses Enter writes back exactly what format() gave
        //them. The horizontals survive that exactly. The elevation crosses a
        //unit conversion in each direction and m→ft→m is not an IEEE identity,
        //so it is pinned tight rather than exact — tight enough that rendering
        //it at a fixed precision (which would drift ~1e-4) fails here.
        //cwFixStationModel::setCoordinateText carries the no-op guarantee that
        //the last ulp can't.
        const auto check = [](double easting, double northing, double elevation,
                              cwUnits::UnitSystem units, cwCoordinateText::AxisOrder order) {
            const QString text = cwCoordinateText::format(easting, northing, elevation,
                                                          units, order);
            const auto result = cwCoordinateText::parse(text, units, order);
            INFO("round tripping \"" << text.toStdString() << "\"");
            REQUIRE_FALSE(result.hasError());
            CHECK(result.value().easting == easting);
            CHECK(result.value().northing == northing);
            CHECK_THAT(result.value().elevation,
                       Catch::Matchers::WithinRel(elevation, 1e-15)
                           || Catch::Matchers::WithinAbs(elevation, 1e-12));
        };

        //Both orders, since a swap that format() and parse() made in lockstep
        //would be invisible to a one-order check.
        for (const auto order : {cwCoordinateText::EastingNorthing,
                                 cwCoordinateText::LatitudeLongitude}) {
            check(46.12113, -115.59902, 92.6592, cwUnits::Metric, order);
            check(46.12113, -115.59902, 92.6592, cwUnits::Imperial, order);
            check(610016.792, 5615117.075, 2545.34, cwUnits::Metric, order);
            check(610016.792, 5615117.075, 2545.34, cwUnits::Imperial, order);
            check(0.0, 0.0, 0.0, cwUnits::Metric, order);
            check(-1.0 / 3.0, 2.0 / 7.0, -123.456, cwUnits::Imperial, order);
        }
    }
}

TEST_CASE("cwCoordinateTextValidator never blocks a keystroke",
          "[FixStation][cwCoordinateText]") {
    cwCoordinateTextValidator validator;

    //Invalid would reject the keystroke outright, and every coordinate is
    //half-typed on its way to being whole.
    const auto state = [&validator](const QString& text) {
        return static_cast<QValidator::State>(validator.validate(text));
    };

    CHECK(state(QStringLiteral("")) == QValidator::Intermediate);
    CHECK(state(QStringLiteral("4")) == QValidator::Intermediate);
    CHECK(state(QStringLiteral("46.12113,")) == QValidator::Intermediate);
    CHECK(state(QStringLiteral("46.12113, -115.59902, 304 b")) == QValidator::Intermediate);
    CHECK(state(QStringLiteral("46.12113, -115.59902")) == QValidator::Acceptable);
    CHECK(state(QStringLiteral("46.12113, -115.59902, 304m")) == QValidator::Acceptable);
}

TEST_CASE("cwCoordinateTextValidator explains the rejection it just made",
          "[FixStation][cwCoordinateText]") {
    cwCoordinateTextValidator validator;

    //CoreClickTextInput shows errorText the instant validate() refuses a
    //commit, so the message has to describe the text it was handed.
    (void)validator.validate(QStringLiteral("46.12113, -115.59902, 304 bananas"));
    CHECK(validator.errorText().contains(QStringLiteral("bananas")));

    (void)validator.validate(QStringLiteral("46.12113"));
    CHECK_FALSE(validator.errorText().contains(QStringLiteral("bananas")));
    CHECK_FALSE(validator.errorText().isEmpty());
}

TEST_CASE("cwCoordinateTextValidator explains itself in its row's axis order",
          "[FixStation][cwCoordinateText]") {
    //FixStationPage shows no other message, so the validator is the only thing
    //that tells a geographic row it wanted a latitude. Hardcoding an order here
    //would send a row on EPSG:4326 a UTM worked example and the wrong axis
    //names — exactly what parse()'s order-aware messages exist to avoid.
    cwCoordinateTextValidator validator;
    CHECK(validator.axisOrder() == cwCoordinateText::EastingNorthing);

    (void)validator.validate(QStringLiteral("46.12113"));
    CHECK(validator.errorText().contains(QStringLiteral("easting")));
    CHECK(validator.errorText().contains(QStringLiteral("610016.792")));

    validator.setAxisOrder(cwCoordinateText::LatitudeLongitude);
    (void)validator.validate(QStringLiteral("46.12113"));
    CHECK(validator.errorText().contains(QStringLiteral("latitude")));
    CHECK(validator.errorText().contains(QStringLiteral("46.12113, -115.59902, 304m")));
    CHECK_FALSE(validator.errorText().contains(QStringLiteral("easting")));
}

TEST_CASE("cwCoordinateTextValidator judges readability, not meaning",
          "[FixStation][cwCoordinateText]") {
    //The verdict is independent of both the unit system and the axis order —
    //they decide what the numbers mean, never whether they can be read — which
    //is what lets the validator stand in Metric for a project it doesn't know.
    //(The *message* is order-dependent; that is the case above.)
    cwCoordinateTextValidator validator;

    for (const auto units : {cwUnits::Metric, cwUnits::Imperial}) {
        for (const auto order : {cwCoordinateText::EastingNorthing,
                                 cwCoordinateText::LatitudeLongitude}) {
            CHECK_FALSE(cwCoordinateText::parse(QStringLiteral("1, 2, 3"), units, order)
                            .hasError());
            CHECK(cwCoordinateText::parse(QStringLiteral("1"), units, order).hasError());
        }
    }

    CHECK(static_cast<QValidator::State>(validator.validate(QStringLiteral("1, 2, 3")))
          == QValidator::Acceptable);
    CHECK(static_cast<QValidator::State>(validator.validate(QStringLiteral("1")))
          == QValidator::Intermediate);
}

TEST_CASE("cwCoordinateText swaps the first two numbers and nothing else",
          "[FixStation][cwCoordinateText]") {
    //The only recovery there is for a coordinate stored with no coordinate
    //system: nothing recorded which axis its text led with, so after a
    //geographic system is named, the user's answer is the only evidence left.
    SECTION("the two horizontals trade places") {
        CHECK(cwCoordinateText::swapHorizontal(QStringLiteral("46.12113, -115.59902, 304m"))
              == QStringLiteral("-115.59902, 46.12113, 304m"));
    }

    SECTION("the elevation stays where it is, unit and all") {
        //The reason this is textual rather than a re-render through format():
        //re-rendering would convert the elevation into the project's units and
        //spell out a new suffix, editing a part of the string nobody asked to
        //move.
        CHECK(cwCoordinateText::swapHorizontal(QStringLiteral("1, 2, 2545.34 feet"))
              == QStringLiteral("2, 1, 2545.34 feet"));
    }

    SECTION("a coordinate that never had an elevation doesn't grow one") {
        //format() always writes three components, so this is the case a
        //re-render would silently turn into an elevation of zero — the
        //distinction §4.1 exists to keep.
        CHECK(cwCoordinateText::swapHorizontal(QStringLiteral("46.12113, -115.59902"))
              == QStringLiteral("-115.59902, 46.12113"));
    }

    SECTION("the separators are the user's, not ours") {
        CHECK(cwCoordinateText::swapHorizontal(QStringLiteral("610016.792  5615117.075  304m"))
              == QStringLiteral("5615117.075  610016.792  304m"));
    }

    SECTION("swapping twice is the string it started as") {
        //Which is the property that makes the offer safe to take: a user who
        //swaps and changes their mind is one swap from where they were, with no
        //rounding or reformatting in between.
        for (const auto& text : {QStringLiteral("46.12113, -115.59902, 304m"),
                                 QStringLiteral("1e3, -2.5, 0"),
                                 QStringLiteral(" 5,6 ")}) {
            CHECK(cwCoordinateText::swapHorizontal(cwCoordinateText::swapHorizontal(text))
                  == text);
        }
    }

    SECTION("digits of different lengths don't corrupt the tail") {
        //The offsets move as the string is rewritten: the longer number lands
        //where the shorter one was. Replacing the earlier span first would
        //leave the later one pointing into the middle of a number.
        CHECK(cwCoordinateText::swapHorizontal(QStringLiteral("1, 5615117.075, 304m"))
              == QStringLiteral("5615117.075, 1, 304m"));
        CHECK(cwCoordinateText::swapHorizontal(QStringLiteral("5615117.075, 1, 304m"))
              == QStringLiteral("1, 5615117.075, 304m"));
    }

    SECTION("text that isn't a coordinate has nothing to swap") {
        //Empty is the answer, not the input back: a caller has to be able to
        //tell "already the way it was" from "there was no swap to make", since
        //the second means the row is Unreadable and says so on its own.
        CHECK(cwCoordinateText::swapHorizontal(QStringLiteral("46.12113")).isEmpty());
        CHECK(cwCoordinateText::swapHorizontal(QString()).isEmpty());

        //Counting numbers is the wrong test, and this is the string that shows
        //it: six of them, no coordinate. Exchanging the first two would rewrite
        //a degrees-minutes-seconds reading into a different one that is just as
        //unreadable, and offer it to the user as a correction.
        CHECK(cwCoordinateText::swapHorizontal(
                  QStringLiteral("N 46 07 16 W 115 35 56")).isEmpty());
    }

    SECTION("what comes out is still readable as a coordinate") {
        //The result is committed straight back through setCoordinateText(), so
        //a swap that produced something the parser refuses would turn a
        //recoverable row into an unreadable one.
        const QString swapped =
            cwCoordinateText::swapHorizontal(QStringLiteral("46.12113, -115.59902, 304m"));
        const auto coordinate = parsed(swapped, cwUnits::Metric,
                                       cwCoordinateText::LatitudeLongitude);
        CHECK(coordinate.northing == tight(-115.59902));
        CHECK(coordinate.easting == tight(46.12113));
    }
}
