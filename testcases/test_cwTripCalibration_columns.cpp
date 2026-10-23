/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwProtoUtils.h"
#include "cwTripCalibration.h"
#include "cavewhere.pb.h"

//Qt includes
#include <QSignalSpy>

TEST_CASE("The LRUD and Splays columns default on and notify when toggled", "[cwTripCalibration]")
{
    cwTripCalibration calibration;
    CHECK(calibration.hasLruds());
    CHECK(calibration.hasSplays());

    QSignalSpy lrudsSpy(&calibration, &cwTripCalibration::lrudsChanged);
    QSignalSpy splaysSpy(&calibration, &cwTripCalibration::splaysChanged);
    QSignalSpy calibrationsSpy(&calibration, &cwTripCalibration::calibrationsChanged);

    calibration.setLruds(false);
    CHECK_FALSE(calibration.hasLruds());
    CHECK(lrudsSpy.size() == 1);
    CHECK(splaysSpy.size() == 0);

    calibration.setLruds(false);
    CHECK(lrudsSpy.size() == 1);

    calibration.setSplays(false);
    CHECK_FALSE(calibration.hasSplays());
    CHECK(splaysSpy.size() == 1);
    CHECK(calibrationsSpy.size() == 2);

    calibration.setLruds(true);
    calibration.setSplays(true);
    CHECK(calibration.hasLruds());
    CHECK(calibration.hasSplays());
    CHECK(lrudsSpy.size() == 2);
    CHECK(splaysSpy.size() == 2);
}

TEST_CASE("The columns ride on the calibration data and its equality", "[cwTripCalibration]")
{
    const cwTripCalibrationData defaults;
    CHECK(defaults.hasLruds());
    CHECK(defaults.hasSplays());

    cwTripCalibrationData noLruds;
    noLruds.setLruds(false);
    CHECK_FALSE(noLruds.hasLruds());
    CHECK(noLruds != defaults);

    cwTripCalibrationData noSplays;
    noSplays.setSplays(false);
    CHECK_FALSE(noSplays.hasSplays());
    CHECK(noSplays != defaults);
    CHECK(noSplays != noLruds);

    cwTripCalibrationData alsoNoSplays;
    alsoNoSplays.setSplays(false);
    CHECK(alsoNoSplays == noSplays);

    cwTripCalibration calibration;
    calibration.setData(noLruds);
    CHECK_FALSE(calibration.hasLruds());
    CHECK(calibration.hasSplays());
    CHECK(calibration.data() == noLruds);
}

TEST_CASE("The columns round trip through the proto", "[cwTripCalibration]")
{
    cwTripCalibration source;
    source.setLruds(false);
    source.setSplays(true);

    CavewhereProto::TripCalibration proto;
    cwProtoUtils::saveTripCalibration(&proto, &source);

    CHECK(proto.has_lruds());
    CHECK_FALSE(proto.lruds());
    CHECK(proto.has_splays());
    CHECK(proto.splays());

    const cwTripCalibrationData loaded = cwProtoUtils::fromProtoTripCalibration(proto);
    CHECK_FALSE(loaded.hasLruds());
    CHECK(loaded.hasSplays());

    source.setLruds(true);
    source.setSplays(false);
    CavewhereProto::TripCalibration flipped;
    cwProtoUtils::saveTripCalibration(&flipped, &source);
    const cwTripCalibrationData flippedLoaded = cwProtoUtils::fromProtoTripCalibration(flipped);
    CHECK(flippedLoaded.hasLruds());
    CHECK_FALSE(flippedLoaded.hasSplays());
}

TEST_CASE("A calibration written before the columns existed loads with both on", "[cwTripCalibration]")
{
    CavewhereProto::TripCalibration proto;
    proto.set_frontsights(true);
    proto.set_backsights(false);
    REQUIRE_FALSE(proto.has_lruds());
    REQUIRE_FALSE(proto.has_splays());

    const cwTripCalibrationData loaded = cwProtoUtils::fromProtoTripCalibration(proto);
    CHECK(loaded.hasLruds());
    CHECK(loaded.hasSplays());
    CHECK_FALSE(loaded.hasBackSights());
}
