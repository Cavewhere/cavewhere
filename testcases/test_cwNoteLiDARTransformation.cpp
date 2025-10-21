// test_cwNoteLiDARTransformation.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

// Qt
#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>
#include <QSignalSpy>

// CaveWhere
#include "cwNoteLiDARTransformation.h"
#include "cwNoteLiDARTransformationData.h"

using Catch::Matchers::WithinAbs;

static inline bool vecApprox(const QVector3D& a, const QVector3D& b, float eps = 1e-4f) {
    CHECK_THAT(a.x(), WithinAbs(b.x(), eps));
    CHECK_THAT(a.y(), WithinAbs(b.y(), eps));
    CHECK_THAT(a.z(), WithinAbs(b.z(), eps));
    return true;
}

static inline QQuaternion fromAxisDeg(const QVector3D& axis, float deg) {
    return QQuaternion::fromAxisAndAngle(axis, deg);
}

// ------------------------------------------------------
// Core Tests
// ------------------------------------------------------
TEST_CASE("cwNoteLiDARTransformation defaults are sane", "[cwNoteLiDARTransformation]") {

    cwNoteLiDARTransformation t;
    REQUIRE(t.upMode() == cwNoteLiDARTransformation::UpMode::ZisUp);
    REQUIRE(t.upCustom().isIdentity());
    REQUIRE_THAT(t.upSign(), WithinAbs(1.0f, 1e-6f)); // default is +1

    const QMatrix4x4 M = t.matrix();
    vecApprox(M.map({1,0,0}), {1,0,0});
    vecApprox(M.map({0,1,0}), {0,1,0});
    vecApprox(M.map({0,0,1}), {0,0,1});
}

// ------------------------------------------------------
// Test each UpMode behavior
// ------------------------------------------------------
TEST_CASE("cwNoteLiDARTransformation UpMode transformations", "[cwNoteLiDARTransformation]") {

    cwNoteLiDARTransformation t;
    t.setNorthUp(0.0);
    t.setScale(1.0);


    SECTION("UpMode::XisUp rotates up to +X") {
        t.setUpMode(cwNoteLiDARTransformation::UpMode::XisUp);
        const QVector3D localUp(1, 0, 0);
        const QVector3D out = t.matrix().map(localUp);
        vecApprox(out, {0,0,1});
    }

    SECTION("UpMode::YisUp keeps up as +Y") {
        t.setUpMode(cwNoteLiDARTransformation::UpMode::YisUp);
        const QVector3D localUp(0, 1, 0);
        const QVector3D out = t.matrix().map(localUp);
        vecApprox(out.normalized(), {0,0,1});
    }

    SECTION("UpMode::ZisUp rotates up to +Z") {
        t.setUpMode(cwNoteLiDARTransformation::UpMode::ZisUp);
        const QVector3D localUp(0, 0, 1);
        const QVector3D out = t.matrix().map(localUp);
        vecApprox(out.normalized(), {0,0,1});
    }

    SECTION("UpMode::Custom uses user quaternion") {
        const QVector3D localUp(.1, .2, .3);
        const QVector3D upZ(0, 0, 1);
        QQuaternion rotation = QQuaternion::rotationTo(localUp, upZ);

        t.setUpMode(cwNoteLiDARTransformation::UpMode::Custom);
        t.setUpCustom(rotation);
        const QVector3D out = t.matrix().map(localUp);
        vecApprox(out.normalized(), upZ);
    }
}

// ------------------------------------------------------
// Test each UpMode behavior (upSign = -1)
// ------------------------------------------------------
TEST_CASE("cwNoteLiDARTransformation UpMode transformations (upSign=-1)", "[cwNoteLiDARTransformation]") {

    cwNoteLiDARTransformation t;
    t.setNorthUp(0.0);
    t.setScale(1.0);
    t.setUpSign(-1.0f); // flip

    SECTION("UpMode::XisUp rotates +X to -Z when upSign=-1") {
        t.setUpMode(cwNoteLiDARTransformation::UpMode::XisUp);
        const QVector3D localUp(1, 0, 0);
        const QVector3D out = t.matrix().map(localUp);
        vecApprox(out.normalized(), {0,0,-1});
    }

    SECTION("UpMode::YisUp rotates +Y to -Z when upSign=-1") {
        t.setUpMode(cwNoteLiDARTransformation::UpMode::YisUp);
        const QVector3D localUp(0, 1, 0);
        const QVector3D out = t.matrix().map(localUp);
        vecApprox(out.normalized(), {0,0,-1});
    }

    SECTION("UpMode::ZisUp maps +Z to -Z when upSign=-1") {
        t.setUpMode(cwNoteLiDARTransformation::UpMode::ZisUp);
        const QVector3D localUp(0, 0, 1);
        const QVector3D out = t.matrix().map(localUp);
        vecApprox(out.normalized(), {0,0,-1});
    }

    SECTION("UpMode::Custom ignores upSign") {
        const QVector3D localUp(.2f, .1f, .7f);
        const QVector3D upZ(0, 0, 1);
        const QQuaternion rotation = QQuaternion::rotationTo(localUp, upZ);

        // Measure output with upSign = -1, then +1; they should be equal.
        t.setUpMode(cwNoteLiDARTransformation::UpMode::Custom);
        t.setUpCustom(rotation);
        const QVector3D outNeg = t.matrix().map(localUp);

        t.setUpSign(+1.0f);
        const QVector3D outPos = t.matrix().map(localUp);

        vecApprox(outNeg.normalized(), upZ);
        vecApprox(outPos.normalized(), upZ);
        vecApprox(outNeg.normalized(), outPos.normalized());
    }
}

// ------------------------------------------------------
// Composition with scale and northUp
// ------------------------------------------------------
TEST_CASE("cwNoteLiDARTransformation composition of upRotation, northUp, and scale", "[cwNoteLiDARTransformation]") {

    cwNoteLiDARTransformation t;
    t.setUpMode(cwNoteLiDARTransformation::UpMode::Custom);
    const QVector3D localUp(.1, .2, .3);
    const QVector3D upZ(0, 0, 1);
    QQuaternion rotation = QQuaternion::rotationTo(localUp, upZ);
    t.setUpCustom(rotation);
    t.setNorthUp(90.0);
    t.setScale(2.0);


    const QMatrix4x4 M = t.matrix();
    // qDebug() << "Inverse:" << M.inverted().map({0.0, 0.0, 1.0});
    //TODO: I'm not 100% sure if these values are correct,
    const QVector3D out = M.map({1.06904, -0.534523, 1.60357});

    qDebug() << "Out:" << out;
    vecApprox(out.normalized(), {0,0,1});
}

// ------------------------------------------------------
// setData / data round-trip
// ------------------------------------------------------
TEST_CASE("cwNoteLiDARTransformation data roundtrip", "[cwNoteLiDARTransformation]") {

    cwNoteLiDARTransformation t;
    cwNoteLiDARTransformationData in;
    in.upMode = cwNoteLiDARTransformationData::UpMode::ZisUp;
    in.upRotation = fromAxisDeg({1,0,0}, 33.0f);
    in.north = 45.0;
    in.scale.scaleNumerator.value = 0.8;

    t.setData(in);
    const auto out = t.data();

    REQUIRE(out.upMode == in.upMode);
    REQUIRE(out.upRotation == in.upRotation);
    REQUIRE_THAT(out.scale.scaleNumerator.value, WithinAbs(in.scale.scaleNumerator.value, 1e-6f));
    REQUIRE_THAT(out.north, WithinAbs(in.north, 1e-6f));
}

// ------------------------------------------------------
// Signal emission
// ------------------------------------------------------
TEST_CASE("cwNoteLiDARTransformation signals fire correctly", "[cwNoteLiDARTransformation]") {
    cwNoteLiDARTransformation t;
    QSignalSpy spyRot(&t, &cwNoteLiDARTransformation::upCustomChanged);
    QSignalSpy spyMode(&t, &cwNoteLiDARTransformation::upModeChanged);

    t.setUpCustom(fromAxisDeg({0,1,0}, 15.0f));
    t.setUpMode(cwNoteLiDARTransformation::UpMode::XisUp);

    REQUIRE(spyRot.count() >= 1);
    REQUIRE(spyMode.count() >= 1);
}
