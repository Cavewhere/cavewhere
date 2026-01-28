//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwTrip.h"
#include "cwCave.h"

//Qt inculdes
#include "cwSignalSpy.h"

TEST_CASE("cwTrip should strip the datestamp away from date", "[cwTrip]") {
    cwTrip trip;

    cwSignalSpy spy(&trip, &cwTrip::dateChanged);

    CHECK(trip.date().time().hour() == 0);
    CHECK(trip.date().time().minute() == 0);
    CHECK(trip.date().time().second() == 0);
    CHECK(trip.date().time().msec() == 0);
    CHECK(trip.date().date() == QDate::currentDate());

    trip.setDate(QDateTime::currentDateTime().addDays(-1));
    CHECK(trip.date().time().hour() == 0);
    CHECK(trip.date().time().minute() == 0);
    CHECK(trip.date().time().second() == 0);
    CHECK(trip.date().time().msec() == 0);
    CHECK(trip.date().date() == QDate::currentDate().addDays(-1));

    CHECK(spy.count() == 1);
}



