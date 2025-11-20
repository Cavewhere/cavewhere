//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Qt includes
#include <QString>

//Our includes
#include "cwClinoReading.h"
#include "cwReading.h"
#include "cwDistanceReading.h"
#include "cwCompassReading.h"
#include "cwMetaTypeSystem.h"

TEST_CASE("cwReading basic functionality", "[cwReading]") {
    cwReading reading("123.45");
    bool ok = false;
    double d = reading.toDouble(&ok);
    CHECK(ok == true);
    CHECK(d == Catch::Approx(123.45));

    // Test equality operator
    cwReading r1("100");
    cwReading r2("100");
    cwReading r3("200");
    CHECK(r1 == r2);
    CHECK_FALSE(r1 == r3);

    // Test conversion from double
    cwReading readingFromDouble;
    readingFromDouble.fromDouble(3.14159);
    double d2 = readingFromDouble.toDouble(&ok);
    CHECK(ok == true);
    CHECK(d2 == Catch::Approx(3.14159));
}

TEST_CASE("cwClinoReading state transitions", "[cwReading]") {
    // Test empty value
    cwClinoReading readingEmpty("");
    CHECK(readingEmpty.state() == cwClinoReading::State::Empty);

    // Test "up" value (case insensitive)
    cwClinoReading readingUp("up");
    CHECK(readingUp.state() == cwClinoReading::State::Up);

    // Test "down" value (case insensitive)
    cwClinoReading readingDown("Down");
    CHECK(readingDown.state() == cwClinoReading::State::Down);

    // Test numeric valid value: dummy validator accepts any non-negative value
    cwClinoReading readingValid("45.6");
    CHECK(readingValid.state() == cwClinoReading::State::Valid);

    // Test invalid non-numeric value (other than "up"/"down")
    cwClinoReading readingInvalid("invalid");
    CHECK(readingInvalid.state() == cwClinoReading::State::Invalid);

    // Test invalid non-numeric value (other than "up"/"down")
    cwClinoReading reading90("90");
    CHECK(reading90.state() == cwClinoReading::State::Valid);

    // Test invalid non-numeric value (other than "up"/"down")
    cwClinoReading reading90digit("90.0000001");
    CHECK(reading90digit.state() == cwClinoReading::State::Invalid);

    // Test invalid non-numeric value (other than "up"/"down")
    cwClinoReading reading90neg("-90");
    CHECK(reading90neg.state() == cwClinoReading::State::Valid);

    // Test invalid non-numeric value (other than "up"/"down")
    cwClinoReading reading90digitNeg("-90.0000001");
    CHECK(reading90digitNeg.state() == cwClinoReading::State::Invalid);

    // Test using the double constructor
    cwClinoReading readingFromDouble(56.78);
    CHECK(readingFromDouble.state() == cwClinoReading::State::Valid);
}

TEST_CASE("cwDistanceReading state transitions", "[cwReading]") {
    // Test empty value
    cwDistanceReading readingEmpty("");
    CHECK(readingEmpty.state() == cwDistanceReading::State::Empty);

    // Test numeric valid value: dummy validator accepts non-negative values
    cwDistanceReading readingValid("12.34");
    CHECK(readingValid.state() == cwDistanceReading::State::Valid);

    // Test numeric valid value: dummy validator accepts negative values
    cwDistanceReading readingNeg("-0.00001");
    CHECK(readingNeg.state() == cwDistanceReading::State::Invalid);

    // Test numeric valid value: dummy validator accepts negative values
    cwDistanceReading readingNegZero("-0.0");
    CHECK(readingNegZero.state() == cwDistanceReading::State::Valid);

    // Test invalid non-numeric value
    cwDistanceReading readingInvalid("abc");
    CHECK(readingInvalid.state() == cwDistanceReading::State::Invalid);

    // Test using the double constructor
    cwDistanceReading readingFromDouble(99.99);
    CHECK(readingFromDouble.state() == cwDistanceReading::State::Valid);
}

TEST_CASE("cwCompassReading state transitions", "[cwReading]") {
    // Test empty value
    cwCompassReading readingEmpty("");
    CHECK(readingEmpty.state() == cwCompassReading::State::Empty);

    // Test numeric valid value within compass range (0-360)
    cwCompassReading readingValid("180");
    CHECK(readingValid.state() == cwCompassReading::State::Valid);

    // Test numeric invalid value outside compass range (e.g., 400)
    cwCompassReading readingInvalid1("400");
    CHECK(readingInvalid1.state() == cwCompassReading::State::Invalid);

    cwCompassReading readingValid360("360.0");
    CHECK(readingValid360.state() == cwCompassReading::State::Valid);

    cwCompassReading readingValid360Invalid("360.00000001");
    CHECK(readingValid360Invalid.state() == cwCompassReading::State::Invalid);

    // Test non-numeric invalid value
    cwCompassReading readingInvalid2("abc");
    CHECK(readingInvalid2.state() == cwCompassReading::State::Invalid);

    // Test using the double constructor with a valid value
    cwCompassReading readingFromDouble(90.0);
    CHECK(readingFromDouble.state() == cwCompassReading::State::Valid);
}

#include <QVariant>

TEST_CASE("QMetaType system convertsion for cwReadings", "[cwReading]") {
    // Valid conversions.
    SECTION("Valid cwReading conversion") {
        cwReading r("123.45");
        QVariant var = QVariant::fromValue(r);
        QString str = var.toString();
        double dbl = var.toDouble();
        CHECK(str == r.value());
        bool ok = false;
        double expected = r.toDouble(&ok);
        CHECK(ok == true);
        CHECK(dbl == Catch::Approx(expected));
    }

    SECTION("Valid cwDistanceReading conversion") {
        cwDistanceReading r("12.34");
        QVariant var = QVariant::fromValue(r);
        QString str = var.toString();
        double dbl = var.toDouble();
        CHECK(str == r.value());
        bool ok = false;
        double expected = r.toDouble(&ok);
        CHECK(ok == true);
        CHECK(dbl == Catch::Approx(expected));
    }

    SECTION("Valid cwCompassReading conversion") {
        cwCompassReading r("180");
        QVariant var = QVariant::fromValue(r);
        QString str = var.toString();
        double dbl = var.toDouble();
        CHECK(str == r.value());
        bool ok = false;
        double expected = r.toDouble(&ok);
        CHECK(ok == true);
        CHECK(dbl == Catch::Approx(expected));
    }

    SECTION("Valid cwClinoReading conversion") {
        cwClinoReading r("45.6");
        QVariant var = QVariant::fromValue(r);
        QString str = var.toString();
        double dbl = var.toDouble();
        CHECK(str == r.value());
        bool ok = false;
        double expected = r.toDouble(&ok);
        // For a valid cwClinoReading, the conversion should succeed.
        CHECK(ok == true);
        CHECK(dbl == Catch::Approx(expected));
    }

    SECTION("Valid cwClinoReading up conversion") {
        cwClinoReading r("uP"); //up value
        QVariant var = QVariant::fromValue(r);
        QString str = var.toString();
        double dbl = var.toDouble(); //Should be 0.0
        CHECK(str == r.value());
        bool ok = false;
        double expected = r.toDouble(&ok);
        // For a valid cwClinoReading, the conversion should succeed.
        CHECK(ok == false);
        CHECK(dbl == 0.0);
        CHECK(dbl == Catch::Approx(expected));
    }

    SECTION("Valid cwClinoReading up conversion") {
        cwClinoReading r("Down"); //down value
        QVariant var = QVariant::fromValue(r);
        QString str = var.toString();
        double dbl = var.toDouble(); //Should be 0.0
        CHECK(str == r.value());
        bool ok = false;
        double expected = r.toDouble(&ok);
        // For a valid cwClinoReading, the conversion should succeed.
        CHECK(ok == false);
        CHECK(dbl == 0.0);
        CHECK(dbl == Catch::Approx(expected));
    }

    // Invalid conversions.
    SECTION("Invalid cwClinoReading conversion") {
        cwClinoReading invalid("invalid");
        QVariant var = QVariant::fromValue(invalid);
        QString str = var.toString();
        double dbl = var.toDouble();
        // The stored string remains unchanged.
        CHECK(str == invalid.value());
        bool ok = false;
        // Direct conversion via the reading should fail.
        invalid.toDouble(&ok);
        CHECK(ok == false);
        // The registered converter should yield 0.0 when the conversion fails.
        CHECK(dbl == Catch::Approx(0.0));
    }

    SECTION("Invalid cwDistanceReading conversion") {
        cwDistanceReading invalid("abc");
        QVariant var = QVariant::fromValue(invalid);
        QString str = var.toString();
        double dbl = var.toDouble();
        CHECK(str == invalid.value());
        bool ok = false;
        invalid.toDouble(&ok);
        CHECK(ok == false);
        CHECK(dbl == Catch::Approx(0.0));
    }

    SECTION("Invalid cwCompassReading conversion") {
        cwCompassReading invalid("abc");
        QVariant var = QVariant::fromValue(invalid);
        QString str = var.toString();
        double dbl = var.toDouble();
        CHECK(str == invalid.value());
        bool ok = false;
        invalid.toDouble(&ok);
        CHECK(ok == false);
        CHECK(dbl == Catch::Approx(0.0));
    }
}
