//Our includes
#include "cwFixStation.h"
#include "cwUnits.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

//Qt includes
#include <QUuid>

//Std includes
#include <cmath>
#include <limits>

namespace {

const QString kUtmZ11N = QStringLiteral("EPSG:32611");
const QString kWgs84 = QStringLiteral("EPSG:4326");

//! \a fix's coordinate, read fresh. Setting a component writes the string but
//! leaves the number it was handed in place, so asserting on that number proves
//! nothing about the string — this is what forces the text to be read back.
cwFixStation reread(const cwFixStation& fix)
{
    cwFixStation copy;
    copy.setInputCS(fix.inputCS());
    copy.setCoordinate(fix.coordinate());
    return copy;
}

}

TEST_CASE("cwFixStation defaults are zero/empty", "[FixStation][cwFixStation]") {
    cwFixStation fix;
    CHECK(fix.stationName().isEmpty());
    CHECK(fix.inputCS().isEmpty());
    CHECK(fix.easting() == 0.0);
    CHECK(fix.northing() == 0.0);
    CHECK(fix.elevation() == 0.0);
    CHECK(fix.horizontalVariance() == 0.0);
    CHECK(fix.verticalVariance() == 0.0);
    CHECK(!fix.id().isNull());
    CHECK(fix.coordinate().isEmpty());
    CHECK(fix.state() == cwFixStation::Empty);
    CHECK_FALSE(fix.hasElevation());
}

TEST_CASE("cwFixStation reads its numbers out of the coordinate it was given",
          "[FixStation][cwFixStation]") {
    SECTION("a projected coordinate leads with the easting") {
        cwFixStation fix;
        fix.setInputCS(kUtmZ11N);
        fix.setCoordinate(QStringLiteral("610016.792, 5615117.075, 304m"));

        CHECK(fix.state() == cwFixStation::Valid);
        CHECK(fix.easting() == 610016.792);
        CHECK(fix.northing() == 5615117.075);
        CHECK(fix.elevation() == 304.0);
        CHECK(fix.hasElevation());
    }

    SECTION("a geographic one leads with the latitude") {
        //The same three numbers under the other CS are a different coordinate,
        //and nothing but the CS says which — 46.12113, -115.59902 is a legal,
        //if absurd, UTM pair.
        cwFixStation fix;
        fix.setInputCS(kWgs84);
        fix.setCoordinate(QStringLiteral("46.12113, -115.59902, 304m"));

        CHECK(fix.northing() == 46.12113);
        CHECK(fix.easting() == -115.59902);
    }

    SECTION("an elevation in feet is converted, because nothing downstream carries a unit") {
        cwFixStation fix;
        fix.setInputCS(kUtmZ11N);
        fix.setCoordinate(QStringLiteral("1, 2, 304ft"));
        //Exactly, not approximately: cwUnits::convert multiplies by the same
        //0.3048 this line does. Approx's default epsilon is relative, so at
        //92 m it would tolerate a millimeter — enough to let a switch to the
        //US survey foot through unnoticed.
        CHECK(fix.elevation() == 304.0 * 0.3048);
        CHECK(fix.hasElevation());
    }

    SECTION("two components mean the elevation was never entered") {
        cwFixStation fix;
        fix.setInputCS(kUtmZ11N);
        fix.setCoordinate(QStringLiteral("610016.792, 5615117.075"));

        CHECK(fix.state() == cwFixStation::Valid);
        CHECK(fix.easting() == 610016.792);
        CHECK_FALSE(fix.hasElevation());
        //Zero either way for everything downstream — survex's *fix takes three
        //numbers — so the distinction is one for the diagnostics to draw.
        CHECK(fix.elevation() == 0.0);
    }

    SECTION("an empty coordinate is Empty, not a coordinate at the origin") {
        //Started from a real coordinate so the zeros below are something the
        //clear actually did. From a fresh fix they are the constructor's, and
        //every one of these checks would pass with the reset deleted.
        cwFixStation fix;
        fix.setInputCS(kUtmZ11N);
        fix.setCoordinate(QStringLiteral("610016.792, 5615117.075, 304m"));
        REQUIRE(fix.easting() == 610016.792);

        fix.setCoordinate(QStringLiteral("   "));
        CHECK(fix.state() == cwFixStation::Empty);
        CHECK(fix.easting() == 0.0);
        CHECK(fix.northing() == 0.0);
        CHECK(fix.elevation() == 0.0);
        CHECK_FALSE(fix.hasElevation());
    }

    SECTION("a coordinate at the origin is entered, and says so") {
        //The distinction the state exists for: some local grids really do put a
        //station at 0, 0, and a row that says so is not an unfilled row.
        cwFixStation fix;
        fix.setInputCS(kUtmZ11N);
        fix.setCoordinate(QStringLiteral("0, 0, 0m"));
        CHECK(fix.state() == cwFixStation::Valid);
    }

    SECTION("text that can't be read is kept, verbatim, and the numbers go with it") {
        //A project is a file someone can edit. Text that doesn't parse is still
        //theirs — it is kept, never dropped and never repaired. The numbers a
        //previous coordinate left behind are not kept: a fix whose text can't
        //be read must not go on plotting where it used to be.
        cwFixStation fix;
        fix.setInputCS(kUtmZ11N);
        fix.setCoordinate(QStringLiteral("610016.792, 5615117.075, 304m"));
        REQUIRE(fix.easting() == 610016.792);

        fix.setCoordinate(QStringLiteral("somewhere over there"));

        CHECK(fix.state() == cwFixStation::Unreadable);
        CHECK(fix.coordinate() == QStringLiteral("somewhere over there"));
        CHECK(fix.easting() == 0.0);
        CHECK(fix.northing() == 0.0);
        CHECK(fix.elevation() == 0.0);
        CHECK_FALSE(fix.hasElevation());
    }

    SECTION("with no coordinate system there is no reading to make, whatever the text says") {
        //NoSystem rather than Unreadable, and the text is what makes the two
        //hard to tell apart: both keep the string and zero the components, and
        //they want opposite messages — *choose a coordinate system* against
        //*this text can't be read*. Nonsense text under no system earns the
        //first, because naming a system is what it is waiting on; the parser
        //has not been consulted and has nothing to say yet.
        cwFixStation fix;
        fix.setCoordinate(QStringLiteral("somewhere over there"));

        CHECK(fix.state() == cwFixStation::NoSystem);
        CHECK(fix.coordinate() == QStringLiteral("somewhere over there"));
        CHECK(fix.easting() == 0.0);
        CHECK(fix.northing() == 0.0);
        CHECK(fix.elevation() == 0.0);
        CHECK_FALSE(fix.hasElevation());

        //Naming one is what lets the text be read at all — and here that only
        //changes which complaint the row earns.
        fix.setInputCS(kUtmZ11N);
        CHECK(fix.state() == cwFixStation::Unreadable);
        CHECK(fix.coordinate() == QStringLiteral("somewhere over there"));
    }

    SECTION("a coordinate system of nothing but spaces is no coordinate system") {
        //The form a hand-edited project arrives in. A blank CaveWhere never
        //writes — saving omits the field entirely — so this only reaches the
        //loader from outside, and a row that reads as Valid off whitespace
        //would be reading a coordinate easting-first on no authority at all.
        cwFixStation fix;
        fix.setInputCS(QStringLiteral("   "));
        fix.setCoordinate(QStringLiteral("610016.792, 5615117.075, 304m"));

        CHECK(fix.state() == cwFixStation::NoSystem);
        CHECK(fix.easting() == 0.0);
    }
}

TEST_CASE("cwFixStation keeps a coordinate written in degrees, minutes and seconds",
          "[FixStation][cwFixStation]") {
    //#654. The string is the only thing stored, so the notation survives by
    //itself — there is no second representation for it to be lost in. What the
    //row reports is decimal, because that is what everything downstream of here
    //carries.
    const QString dms = QStringLiteral("46°07'16.1\" N, 115°35'56.5\" W, 304m");
    //What those angles come to, accumulated the way parse() accumulates them so
    //the two agree to the last bit.
    const double latitude = 46.0 + 7.0 / 60.0 + 16.1 / 3600.0;
    const double longitude = -(115.0 + 35.0 / 60.0 + 56.5 / 3600.0);

    cwFixStation fix;
    fix.setInputCS(kWgs84);
    fix.setCoordinate(dms);

    CHECK(fix.state() == cwFixStation::Valid);
    CHECK(fix.coordinate() == dms);
    CHECK(fix.northing() == latitude);
    CHECK(fix.easting() == longitude);
    CHECK(fix.elevation() == 304.0);

    SECTION("and reading it again reads the same thing") {
        //Writing a component would spell the coordinate back out in decimal, so
        //this is the property that says nothing did.
        CHECK(reread(fix).coordinate() == dms);
        CHECK(reread(fix).northing() == fix.northing());
    }

    SECTION("a projected system leaves it Unreadable rather than reinterpreting it") {
        //An angle is a latitude and a longitude. Reading 46°07'16" as an easting
        //would put the station 46 m from the origin of a grid in meters, which is
        //a worse answer than saying the row can't be read.
        fix.setInputCS(kUtmZ11N);
        CHECK(fix.state() == cwFixStation::Unreadable);
        CHECK(fix.coordinate() == dms);
        CHECK(fix.easting() == 0.0);
        CHECK(fix.northing() == 0.0);

        //And naming a geographic one again reads it straight back: the text was
        //kept, so nothing was lost in between.
        fix.setInputCS(kWgs84);
        CHECK(fix.state() == cwFixStation::Valid);
        CHECK(fix.northing() == latitude);
    }

    SECTION("with no system at all it is NoSystem, like any other text") {
        //There is no axis order to read it under, and the parser is never
        //consulted — so this row wants "choose a coordinate system" whatever its
        //notation is.
        cwFixStation noSystem;
        noSystem.setCoordinate(dms);
        CHECK(noSystem.state() == cwFixStation::NoSystem);
        CHECK(noSystem.coordinate() == dms);
    }
}

TEST_CASE("cwFixStation re-reads its coordinate when the coordinate system changes",
          "[FixStation][cwFixStation]") {
    //The whole point of deriving: a coordinate means what its own text says
    //under the system it is read in, so correcting the system corrects the fix
    //rather than leaving the numbers where a previous reading put them.
    cwFixStation fix;
    fix.setInputCS(kWgs84);
    fix.setCoordinate(QStringLiteral("610016.792, 5615117.075, 304m"));
    REQUIRE(fix.northing() == 610016.792);

    fix.setInputCS(kUtmZ11N);
    CHECK(fix.easting() == 610016.792);
    CHECK(fix.northing() == 5615117.075);
    CHECK(fix.coordinate() == QStringLiteral("610016.792, 5615117.075, 304m"));

    SECTION("but a change between two projected systems moves nothing") {
        //Same axis order, so there is nothing to re-read differently. The
        //wrong-UTM-zone case has no axis question at all.
        fix.setInputCS(QStringLiteral("EPSG:32613"));
        CHECK(fix.easting() == 610016.792);
        CHECK(fix.northing() == 5615117.075);
    }
}

TEST_CASE("cwFixStation writes the coordinate back out when a component is set",
          "[FixStation][cwFixStation]") {
    //The importers have numbers rather than a string, and there is only one
    //place to put a number now. Spelling it out in the setter rather than at the
    //call sites is what makes the rule hold for call sites that don't exist yet.
    SECTION("the three numbers round-trip through the string they produce") {
        cwFixStation fix;
        fix.setInputCS(kUtmZ11N);
        fix.setEasting(610016.792);
        fix.setNorthing(5615117.075);
        fix.setElevation(304.0);

        CHECK(fix.coordinate() == QStringLiteral("610016.792, 5615117.075, 304.000m"));

        const cwFixStation read = reread(fix);
        CHECK(read.easting() == 610016.792);
        CHECK(read.northing() == 5615117.075);
        CHECK(read.elevation() == 304.0);
    }

    SECTION("under a geographic CS too, where the string leads with the latitude") {
        //cwWallsImporter writes longitude into the easting and latitude into the
        //northing under a geographic CS. It looks transposed and is not: the
        //string comes out "lat, lon" and reads straight back.
        cwFixStation fix;
        fix.setInputCS(kWgs84);
        fix.setEasting(-105.27);
        fix.setNorthing(40.015);
        fix.setElevation(1655.0);

        CHECK(fix.coordinate() == QStringLiteral("40.015, -105.27, 1655.000m"));

        const cwFixStation read = reread(fix);
        CHECK(read.easting() == -105.27);
        CHECK(read.northing() == 40.015);
    }

    SECTION("awkward doubles survive the round trip") {
        //shortestNumber() writes the shortest text that reads back as the same
        //double, so the horizontals are exact rather than close — a fixed
        //precision would lose the low bits of one of these. The elevation is
        //the one component written to a display precision instead: a fix is
        //stored in meters, so it keeps the millimeter and nothing finer.
        const QList<double> awkward = {
            0.1, 1.0 / 3.0, 46.121129999999997, 1e-7, 123456789.123456789, -0.0000001234
        };
        const double halfMillimeter =
            0.5 * std::pow(10.0, -cwUnits::lengthDecimals(cwUnits::Meters));
        for (double value : awkward) {
            cwFixStation fix;
            fix.setInputCS(kUtmZ11N);
            fix.setEasting(value);
            fix.setNorthing(value);
            fix.setElevation(value);

            const cwFixStation read = reread(fix);
            CHECK(read.easting() == value);
            CHECK(read.northing() == value);
            CHECK_THAT(read.elevation(), Catch::Matchers::WithinAbs(value, halfMillimeter));
        }
    }

    SECTION("a component written onto a two-component coordinate enters an elevation") {
        cwFixStation fix;
        fix.setInputCS(kUtmZ11N);
        fix.setCoordinate(QStringLiteral("1, 2"));
        REQUIRE_FALSE(fix.hasElevation());

        fix.setEasting(9.0);
        CHECK(fix.coordinate() == QStringLiteral("9, 2, 0.000m"));

        //Read back out of the string rather than off the fix: an elevation the
        //setter merely left at zero and one the string actually spells out look
        //identical from here otherwise.
        const cwFixStation read = reread(fix);
        CHECK(read.hasElevation());
        CHECK(read.elevation() == 0.0);
    }

    SECTION("a component that can't be written as a coordinate leaves the fix Unreadable") {
        //format() renders a non-finite double as "inf", which parse() won't
        //take. The state has to follow the string it actually wrote rather than
        //assert what it meant to write — otherwise the fix claims to be Valid
        //while its components say one thing and its coordinate says another,
        //and operator==, which compares only the string, would equate two fixes
        //sitting in different places.
        cwFixStation fix;
        fix.setInputCS(kUtmZ11N);
        fix.setEasting(std::numeric_limits<double>::infinity());

        CHECK(fix.state() == cwFixStation::Unreadable);
        CHECK(fix.easting() == 0.0);
    }

    SECTION("the CS has to be set first, and a component written before it reads back as nothing") {
        //The precondition inputCS() documents, exercised in the order that
        //breaks it. Writing components while the CS is still empty used to
        //spell them out easting-first, so setting a geographic CS afterwards
        //read that same text latitude-first and the two horizontals swapped
        //silently. Now there is no axis order to guess with, so the numbers
        //never come back at all — the row says it has no system instead of
        //quietly reporting a coordinate somewhere else.
        cwFixStation late;
        late.setEasting(-105.27);
        late.setNorthing(40.015);
        CHECK(late.state() == cwFixStation::NoSystem);
        CHECK(late.easting() == 0.0);
        CHECK(late.northing() == 0.0);

        //The same numbers written in the order the importers use.
        cwFixStation early;
        early.setInputCS(kWgs84);
        early.setEasting(-105.27);
        early.setNorthing(40.015);
        CHECK(early.easting() == -105.27);
        CHECK(early.northing() == 40.015);
    }

    SECTION("three numbers at once survive a fix that has no CS to read them under") {
        //What a caller holding all three should use, and the reason it exists:
        //writing them one at a time here would keep only the last, since each
        //write reads its own string back and finds no axis order to read it
        //under. The svx importer and the project loader both hit this — a *fix
        //with no *cs is an ordinary local grid.
        cwFixStation fix;
        fix.setCoordinate(610016.792, 5615117.075, 304.0);

        CHECK(fix.state() == cwFixStation::NoSystem);
        CHECK(fix.coordinate() == QStringLiteral("610016.792, 5615117.075, 304.000m"));
        CHECK(fix.easting() == 0.0);

        //And naming the system they were written in reads all three back.
        fix.setInputCS(kUtmZ11N);
        CHECK(fix.state() == cwFixStation::Valid);
        CHECK(fix.easting() == 610016.792);
        CHECK(fix.northing() == 5615117.075);
        CHECK(fix.elevation() == 304.0);
    }
}

TEST_CASE("cwFixStation reads a stored elevation in meters whatever the project displays",
          "[FixStation][cwFixStation]") {
    //Load-bearing: a stored elevation always spells its own unit out, so there
    //is nothing here for a unit system to resolve — and a fix's *meaning* must
    //not move when the project's display units change. There is deliberately no
    //way to hand this class a unit system.
    //What the component setters write is in meters, so a number set directly
    //comes back as itself rather than one foot-conversion away. (That a "ft"
    //suffix is honored on the way in is covered above.)
    cwFixStation written;
    written.setInputCS(kUtmZ11N);
    written.setElevation(304.0);
    CHECK(written.coordinate().endsWith(cwUnits::unitName(cwUnits::Meters)));
    CHECK(reread(written).elevation() == 304.0);
}

TEST_CASE("cwFixStation setters round-trip", "[FixStation][cwFixStation]") {
    cwFixStation fix;
    const QUuid id = QUuid::createUuid();
    fix.setId(id);
    fix.setStationName(QStringLiteral("A1"));
    fix.setInputCS(QStringLiteral("EPSG:32612"));
    fix.setEasting(500123.456);
    fix.setNorthing(4194567.89);
    fix.setElevation(2750.5);
    fix.setHorizontalVariance(0.5);
    fix.setVerticalVariance(1.0);

    CHECK(fix.id() == id);
    CHECK(fix.stationName() == QStringLiteral("A1"));
    CHECK(fix.inputCS() == QStringLiteral("EPSG:32612"));
    CHECK(fix.horizontalVariance() == 0.5);
    CHECK(fix.verticalVariance() == 1.0);

    //Through the string, because that is where the three components now live —
    //asking the fix for the number it was just handed would pass with the
    //coordinate never written at all. The non-component setters above must
    //also leave that string alone.
    const cwFixStation read = reread(fix);
    CHECK(read.easting() == 500123.456);
    CHECK(read.northing() == 4194567.89);
    CHECK(read.elevation() == 2750.5);
}

TEST_CASE("cwFixStation copy semantics: COW", "[FixStation][cwFixStation]") {
    cwFixStation a;
    a.setStationName(QStringLiteral("A1"));
    a.setInputCS(kUtmZ11N);
    a.setEasting(123.0);

    cwFixStation b = a;
    CHECK(b.stationName() == QStringLiteral("A1"));
    CHECK(b.easting() == 123.0);
    CHECK(a == b);

    b.setEasting(999.0);
    CHECK(a.easting() == 123.0);
    CHECK(b.easting() == 999.0);
    CHECK(a != b);
}

TEST_CASE("cwFixStation equality compares all fields", "[FixStation][cwFixStation]") {
    cwFixStation a;
    cwFixStation b;
    b.setId(a.id());
    CHECK(a == b);

    b.setNorthing(1.0);
    CHECK(a != b);

    SECTION("including the coordinate, which is what carries the numbers") {
        cwFixStation c;
        cwFixStation d;
        d.setId(c.id());
        c.setCoordinate(QStringLiteral("1, 2, 3m"));
        CHECK(c != d);

        d.setCoordinate(QStringLiteral("1, 2, 3m"));
        CHECK(c == d);
    }

    SECTION("and the CS, because the same string under another one is another place") {
        cwFixStation c;
        cwFixStation d;
        d.setId(c.id());
        c.setInputCS(kUtmZ11N);
        c.setCoordinate(QStringLiteral("46.12113, -115.59902, 3m"));
        d.setInputCS(kWgs84);
        d.setCoordinate(QStringLiteral("46.12113, -115.59902, 3m"));

        REQUIRE(c.easting() != d.easting());
        CHECK(c != d);
    }
}
