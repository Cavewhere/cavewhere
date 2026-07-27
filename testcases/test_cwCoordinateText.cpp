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
