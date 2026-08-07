/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwGeoReference.h"

//Qt includes
#include <QSignalSpy>
#include <QUuid>

namespace {
    const QString kLocalCS = QStringLiteral(
        "+proj=tmerc +lat_0=37.1832 +lon_0=-84.0947 +k=1 +x_0=0 +y_0=0 "
        "+datum=WGS84 +units=m +no_defs +type=crs");
    const QString kOtherLocalCS = QStringLiteral(
        "+proj=tmerc +lat_0=38.0 +lon_0=-85.0 +k=1 +x_0=0 +y_0=0 "
        "+datum=WGS84 +units=m +no_defs +type=crs");

    cwGeoReference::Anchor fixAnchor()
    {
        return {cwGeoReference::Anchor::FixStation, QUuid::createUuid()};
    }
}

TEST_CASE("cwGeoReference starts with no local projection", "[cwGeoReference]")
{
    cwGeoReference reference;
    CHECK(reference.state() == cwGeoReference::Ungeoreferenced);
    CHECK(reference.localCoordinateSystem().isEmpty());
    CHECK_FALSE(reference.anchor().isValid());
    CHECK(reference.verticalDatum().isEmpty());
}

TEST_CASE("cwGeoReference::anchorTo writes the frame and its anchor together",
          "[cwGeoReference]")
{
    cwGeoReference reference;
    QSignalSpy spy(&reference, &cwGeoReference::localProjectionChanged);

    const auto anchor = fixAnchor();
    reference.anchorTo(anchor, kLocalCS);

    CHECK(reference.state() == cwGeoReference::Anchored);
    CHECK(reference.localCoordinateSystem() == kLocalCS);
    CHECK(reference.anchor() == anchor);
    CHECK(spy.count() == 1);

    SECTION("re-anchoring to the same frame and anchor says nothing")
    {
        reference.anchorTo(anchor, kLocalCS);
        CHECK(spy.count() == 1);
    }

    SECTION("a corrected anchor location moves the frame")
    {
        reference.anchorTo(anchor, kOtherLocalCS);
        CHECK(reference.localCoordinateSystem() == kOtherLocalCS);
        CHECK(reference.anchor() == anchor);
        CHECK(spy.count() == 2);
    }

    SECTION("a new anchor takes over the frame it derived")
    {
        const auto lazAnchor = cwGeoReference::Anchor{cwGeoReference::Anchor::LazLayer,
                                                      QUuid::createUuid()};
        reference.anchorTo(lazAnchor, kOtherLocalCS);
        CHECK(reference.anchor() == lazAnchor);
        CHECK(reference.localCoordinateSystem() == kOtherLocalCS);
    }
}

TEST_CASE("cwGeoReference reports a moved frame apart from a moved anchor",
          "[cwGeoReference]")
{
    // Re-decoding a directory of point clouds hangs off the narrower signal, so
    // the two have to come apart: everything a cloud was transformed into is
    // still true when only the anchor changes.
    cwGeoReference reference;
    QSignalSpy projectionSpy(&reference, &cwGeoReference::localProjectionChanged);
    QSignalSpy frameSpy(&reference, &cwGeoReference::localCoordinateSystemChanged);

    const auto anchor = fixAnchor();
    reference.anchorTo(anchor, kLocalCS);
    CHECK(projectionSpy.count() == 1);
    CHECK(frameSpy.count() == 1);

    SECTION("the same frame under a different anchor is not a move")
    {
        reference.anchorTo(fixAnchor(), kLocalCS);
        CHECK(projectionSpy.count() == 2);
        CHECK(frameSpy.count() == 1);
    }

    SECTION("freezing keeps every coordinate where it was")
    {
        reference.freeze();
        CHECK(reference.state() == cwGeoReference::Frozen);
        CHECK(projectionSpy.count() == 2);
        CHECK(frameSpy.count() == 1);
    }

    SECTION("a re-derived frame is a move")
    {
        reference.anchorTo(anchor, kOtherLocalCS);
        CHECK(projectionSpy.count() == 2);
        CHECK(frameSpy.count() == 2);
    }

    SECTION("losing the frame is a move")
    {
        reference.clear();
        CHECK(reference.localCoordinateSystem().isEmpty());
        CHECK(projectionSpy.count() == 2);
        CHECK(frameSpy.count() == 2);
    }
}

TEST_CASE("cwGeoReference::anchorTo refuses half an anchoring", "[cwGeoReference]")
{
    // Either half alone would leave a state naming an input the frame didn't
    // come from, and the next edit to that input would move the origin for
    // nothing.
    cwGeoReference reference;
    QSignalSpy spy(&reference, &cwGeoReference::localProjectionChanged);

    SECTION("no frame")
    {
        reference.anchorTo(fixAnchor(), QString());
    }

    SECTION("no anchor")
    {
        reference.anchorTo(cwGeoReference::Anchor{}, kLocalCS);
    }

    SECTION("an anchor kind with no id")
    {
        reference.anchorTo({cwGeoReference::Anchor::FixStation, QUuid()}, kLocalCS);
    }

    CHECK(reference.state() == cwGeoReference::Ungeoreferenced);
    CHECK(reference.localCoordinateSystem().isEmpty());
    CHECK(spy.count() == 0);
}

TEST_CASE("cwGeoReference::freeze keeps the frame and drops the anchor",
          "[cwGeoReference]")
{
    cwGeoReference reference;
    reference.anchorTo(fixAnchor(), kLocalCS);

    QSignalSpy spy(&reference, &cwGeoReference::localProjectionChanged);
    reference.freeze();

    CHECK(reference.state() == cwGeoReference::Frozen);
    CHECK(reference.localCoordinateSystem() == kLocalCS);
    CHECK_FALSE(reference.anchor().isValid());
    CHECK(spy.count() == 1);

    SECTION("freezing twice says nothing")
    {
        reference.freeze();
        CHECK(spy.count() == 1);
    }
}

TEST_CASE("cwGeoReference::freeze has nothing to freeze without a frame",
          "[cwGeoReference]")
{
    cwGeoReference reference;
    QSignalSpy spy(&reference, &cwGeoReference::localProjectionChanged);

    reference.freeze();

    CHECK(reference.state() == cwGeoReference::Ungeoreferenced);
    CHECK(spy.count() == 0);
}

TEST_CASE("cwGeoReference::clear returns to ungeoreferenced", "[cwGeoReference]")
{
    cwGeoReference reference;
    reference.anchorTo(fixAnchor(), kLocalCS);

    QSignalSpy spy(&reference, &cwGeoReference::localProjectionChanged);
    reference.clear();

    CHECK(reference.state() == cwGeoReference::Ungeoreferenced);
    CHECK(reference.localCoordinateSystem().isEmpty());
    CHECK_FALSE(reference.anchor().isValid());
    CHECK(spy.count() == 1);

    SECTION("clearing an already-clear reference says nothing")
    {
        reference.clear();
        CHECK(spy.count() == 1);
    }
}

TEST_CASE("cwGeoReference::restore reproduces what was written", "[cwGeoReference]")
{
    cwGeoReference reference;
    const auto anchor = fixAnchor();

    reference.restore(cwGeoReference::Anchored, kLocalCS, anchor,
                      QStringLiteral("NAVD88"));

    CHECK(reference.state() == cwGeoReference::Anchored);
    CHECK(reference.localCoordinateSystem() == kLocalCS);
    CHECK(reference.anchor() == anchor);
    CHECK(reference.verticalDatum() == QStringLiteral("NAVD88"));
}

TEST_CASE("cwGeoReference::restore repairs a state that can't be true",
          "[cwGeoReference]")
{
    cwGeoReference reference;

    SECTION("a state with no frame isn't georeferenced, whatever it claims")
    {
        reference.restore(cwGeoReference::Anchored, QString(), fixAnchor(), QString());
        CHECK(reference.state() == cwGeoReference::Ungeoreferenced);
        CHECK(reference.localCoordinateSystem().isEmpty());
        CHECK_FALSE(reference.anchor().isValid());
    }

    SECTION("anchored to nothing is frozen")
    {
        reference.restore(cwGeoReference::Anchored, kLocalCS, cwGeoReference::Anchor{},
                          QString());
        CHECK(reference.state() == cwGeoReference::Frozen);
        CHECK(reference.localCoordinateSystem() == kLocalCS);
    }

    SECTION("a frame nothing claims to be in is frozen, not discarded")
    {
        reference.restore(cwGeoReference::Ungeoreferenced, kLocalCS,
                          cwGeoReference::Anchor{}, QString());
        CHECK(reference.state() == cwGeoReference::Frozen);
        CHECK(reference.localCoordinateSystem() == kLocalCS);
    }

    SECTION("an anchor a frozen state has no use for is dropped")
    {
        reference.restore(cwGeoReference::Frozen, kLocalCS, fixAnchor(), QString());
        CHECK(reference.state() == cwGeoReference::Frozen);
        CHECK_FALSE(reference.anchor().isValid());
    }
}

TEST_CASE("cwGeoReference records a vertical datum without applying it",
          "[cwGeoReference]")
{
    cwGeoReference reference;
    QSignalSpy spy(&reference, &cwGeoReference::verticalDatumChanged);

    reference.setVerticalDatum(QStringLiteral("NAVD88"));
    CHECK(reference.verticalDatum() == QStringLiteral("NAVD88"));
    CHECK(spy.count() == 1);

    reference.setVerticalDatum(QStringLiteral("NAVD88"));
    CHECK(spy.count() == 1);

    // It is metadata about the elevations, not part of the frame — moving the
    // frame doesn't change what the elevations are heights above.
    reference.anchorTo(fixAnchor(), kLocalCS);
    CHECK(reference.verticalDatum() == QStringLiteral("NAVD88"));
    CHECK(spy.count() == 1);
}

TEST_CASE("cwGeoReference describes the frame it is holding", "[cwGeoReference]")
{
    cwGeoReference reference;

    SECTION("nothing to describe without a frame")
    {
        CHECK(reference.datumName().isEmpty());
        CHECK_FALSE(reference.hasOrigin());
    }

    SECTION("a frame answers with its datum and where it is centered")
    {
        reference.anchorTo(fixAnchor(), kLocalCS);

        CHECK(reference.datumName().contains(QStringLiteral("World Geodetic System 1984")));
        REQUIRE(reference.hasOrigin());
        CHECK(reference.originLatitude() == Catch::Approx(37.1832));
        CHECK(reference.originLongitude() == Catch::Approx(-84.0947));
    }

    SECTION("the description follows the frame when it moves")
    {
        reference.anchorTo(fixAnchor(), kLocalCS);
        reference.recenter(kOtherLocalCS);

        CHECK(reference.originLatitude() == Catch::Approx(38.0));
        CHECK(reference.originLongitude() == Catch::Approx(-85.0));
    }

    SECTION("and goes away with it")
    {
        reference.anchorTo(fixAnchor(), kLocalCS);
        reference.clear();

        CHECK(reference.datumName().isEmpty());
        CHECK_FALSE(reference.hasOrigin());
        CHECK(reference.originLatitude() == 0.0);
        CHECK(reference.originLongitude() == 0.0);
    }

    SECTION("a restored frame describes itself the same way a derived one does")
    {
        // The load path writes the frame without replaying the transitions that
        // produced it, so it has to arrive described all the same.
        reference.restore(cwGeoReference::Frozen, kLocalCS, cwGeoReference::Anchor{},
                          QStringLiteral("NAVD88"));

        CHECK(reference.datumName().contains(QStringLiteral("World Geodetic System 1984")));
        REQUIRE(reference.hasOrigin());
        CHECK(reference.originLatitude() == Catch::Approx(37.1832));
    }
}
