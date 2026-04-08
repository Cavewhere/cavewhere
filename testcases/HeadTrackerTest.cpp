//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

//Qt includes
#include <QSignalSpy>
#include <QVector3D>
#include <QQuaternion>

//Our includes
#include "cwAbstractHeadTracker.h"

// Mock tracker for testing the abstract base class
class MockHeadTracker : public cwAbstractHeadTracker
{
    Q_OBJECT

public:
    using cwAbstractHeadTracker::cwAbstractHeadTracker;

    bool isAvailable() const override { return m_available; }

    void setAvailable(bool available)
    {
        m_available = available;
        emit availableChanged();
    }

    // Expose protected methods for testing
    using cwAbstractHeadTracker::setRawEyePosition;
    using cwAbstractHeadTracker::setRawHeadRotation;

protected:
    void startTracking() override { m_trackingStarted = true; }
    void stopTracking() override { m_trackingStarted = false; }

public:
    bool m_available = true;
    bool m_trackingStarted = false;
};

TEST_CASE("cwAbstractHeadTracker initial state", "[HeadTracker]")
{
    MockHeadTracker tracker;

    CHECK(tracker.isRunning() == false);
    CHECK(tracker.smoothing() == 0.7);
    CHECK(tracker.eyePosition() == QVector3D());
    CHECK(tracker.headRotation() == QQuaternion());
}

TEST_CASE("cwAbstractHeadTracker start/stop lifecycle", "[HeadTracker]")
{
    MockHeadTracker tracker;
    QSignalSpy runningSpy(&tracker, &MockHeadTracker::runningChanged);

    SECTION("Start when available")
    {
        tracker.start();
        CHECK(tracker.isRunning() == true);
        CHECK(tracker.m_trackingStarted == true);
        CHECK(runningSpy.count() == 1);

        tracker.stop();
        CHECK(tracker.isRunning() == false);
        CHECK(tracker.m_trackingStarted == false);
        CHECK(runningSpy.count() == 2);
    }

    SECTION("Start when not available emits error")
    {
        tracker.setAvailable(false);
        QSignalSpy errorSpy(&tracker, &MockHeadTracker::errorOccurred);

        tracker.start();
        CHECK(tracker.isRunning() == false);
        CHECK(errorSpy.count() == 1);
    }

    SECTION("Double start is no-op")
    {
        tracker.start();
        tracker.start();
        CHECK(runningSpy.count() == 1);
    }

    SECTION("Double stop is no-op")
    {
        tracker.start();
        tracker.stop();
        tracker.stop();
        CHECK(runningSpy.count() == 2);
    }
}

TEST_CASE("cwAbstractHeadTracker smoothing property", "[HeadTracker]")
{
    MockHeadTracker tracker;
    QSignalSpy smoothingSpy(&tracker, &MockHeadTracker::smoothingChanged);

    SECTION("Set valid smoothing")
    {
        tracker.setSmoothing(0.8);
        CHECK_THAT(tracker.smoothing(), Catch::Matchers::WithinAbs(0.8, 1e-6));
        CHECK(smoothingSpy.count() == 1);
    }

    SECTION("Clamp to [0, 1]")
    {
        tracker.setSmoothing(-0.5);
        CHECK_THAT(tracker.smoothing(), Catch::Matchers::WithinAbs(0.0, 1e-6));

        tracker.setSmoothing(1.5);
        CHECK_THAT(tracker.smoothing(), Catch::Matchers::WithinAbs(1.0, 1e-6));
    }

    SECTION("Same value does not emit")
    {
        tracker.setSmoothing(0.7); // already 0.7
        CHECK(smoothingSpy.count() == 0);
    }
}

TEST_CASE("cwAbstractHeadTracker EMA smoothing filter", "[HeadTracker]")
{
    MockHeadTracker tracker;
    tracker.start();

    SECTION("No smoothing passes through raw values")
    {
        tracker.setSmoothing(0.0);
        QSignalSpy posSpy(&tracker, &MockHeadTracker::eyePositionChanged);

        QVector3D pos1(0.1f, 0.2f, 0.5f);
        tracker.setRawEyePosition(pos1);
        REQUIRE(posSpy.count() == 1);
        CHECK(tracker.eyePosition() == pos1);

        QVector3D pos2(0.3f, 0.4f, 0.6f);
        tracker.setRawEyePosition(pos2);
        REQUIRE(posSpy.count() == 2);
        CHECK(tracker.eyePosition() == pos2);
    }

    SECTION("Maximum smoothing heavily weights previous value")
    {
        tracker.setSmoothing(1.0);
        QVector3D pos1(0.1f, 0.2f, 0.5f);
        tracker.setRawEyePosition(pos1);

        // With smoothing=1.0, alpha=0 → new value = previous entirely.
        // But first value has no previous, so it should be pos1.
        CHECK(tracker.eyePosition() == pos1);

        QVector3D pos2(1.0f, 1.0f, 1.0f);
        tracker.setRawEyePosition(pos2);
        // With alpha=0, result should still be pos1
        CHECK(tracker.eyePosition() == pos1);
    }

    SECTION("Medium smoothing blends values")
    {
        tracker.setSmoothing(0.5);
        QVector3D pos1(0.0f, 0.0f, 0.5f);
        tracker.setRawEyePosition(pos1);
        CHECK(tracker.eyePosition() == pos1);

        QVector3D pos2(1.0f, 0.0f, 0.5f);
        tracker.setRawEyePosition(pos2);

        // alpha = 1 - 0.5 = 0.5
        // result = prev * 0.5 + raw * 0.5 = (0,0,0.5)*0.5 + (1,0,0.5)*0.5 = (0.5, 0, 0.5)
        QVector3D expected(0.5f, 0.0f, 0.5f);
        CHECK(tracker.eyePosition() == expected);
    }
}

TEST_CASE("cwAbstractHeadTracker signals emit correctly", "[HeadTracker]")
{
    MockHeadTracker tracker;
    tracker.setSmoothing(0.0); // No smoothing for predictable testing
    tracker.start();

    QSignalSpy posSpy(&tracker, &MockHeadTracker::eyePositionChanged);
    QSignalSpy rotSpy(&tracker, &MockHeadTracker::headRotationChanged);

    QVector3D pos(0.1f, 0.2f, 0.5f);
    tracker.setRawEyePosition(pos);
    CHECK(posSpy.count() == 1);
    CHECK(posSpy.first().first().value<QVector3D>() == pos);

    QQuaternion rot = QQuaternion::fromEulerAngles(10.0f, 20.0f, 0.0f);
    tracker.setRawHeadRotation(rot);
    CHECK(rotSpy.count() == 1);
}

TEST_CASE("cwAbstractHeadTracker same position does not re-emit", "[HeadTracker]")
{
    MockHeadTracker tracker;
    tracker.setSmoothing(0.0);
    tracker.start();

    QSignalSpy posSpy(&tracker, &MockHeadTracker::eyePositionChanged);

    QVector3D pos(0.1f, 0.2f, 0.5f);
    tracker.setRawEyePosition(pos);
    tracker.setRawEyePosition(pos);
    CHECK(posSpy.count() == 1);
}

#include "HeadTrackerTest.moc"
