/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch2 includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwProtoUtils.h"
#include "cwTripCalibration.h"
#include "cavewhere.pb.h"

TEST_CASE("cwProtoUtils: round-trips autoDeclination + declinationManual", "[cwProtoUtils_Auto]")
{
    cwTripCalibration source;
    source.setAutoDeclination(true);
    source.setDeclinationManual(8.75);

    CavewhereProto::TripCalibration proto;
    cwProtoUtils::saveTripCalibration(&proto, &source);

    REQUIRE(proto.has_auto_declination());
    CHECK(proto.auto_declination() == true);
    CHECK(proto.declination() == 8.75);

    const cwTripCalibrationData loaded = cwProtoUtils::fromProtoTripCalibration(proto);
    CHECK(loaded.autoDeclination() == true);
    CHECK(loaded.declinationManual() == 8.75);
}

TEST_CASE("cwProtoUtils: round-trips autoDeclination=false", "[cwProtoUtils_Auto]")
{
    cwTripCalibration source;
    source.setAutoDeclination(false);
    source.setDeclinationManual(-3.5);

    CavewhereProto::TripCalibration proto;
    cwProtoUtils::saveTripCalibration(&proto, &source);

    REQUIRE(proto.has_auto_declination());
    CHECK(proto.auto_declination() == false);

    const cwTripCalibrationData loaded = cwProtoUtils::fromProtoTripCalibration(proto);
    CHECK(loaded.autoDeclination() == false);
    CHECK(loaded.declinationManual() == -3.5);
}

TEST_CASE("cwProtoUtils: legacy proto (no auto_declination field) loads with autoDeclination=false", "[cwProtoUtils_Auto]")
{
    // A pre-feature project would have declination set but auto_declination
    // absent. Loading must NOT silently shift bearings → force auto off.
    CavewhereProto::TripCalibration proto;
    proto.set_declination(11.0);
    REQUIRE_FALSE(proto.has_auto_declination());

    const cwTripCalibrationData loaded = cwProtoUtils::fromProtoTripCalibration(proto);
    CHECK(loaded.autoDeclination() == false);
    CHECK(loaded.declinationManual() == 11.0);
}
