//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

//Qt includes
#include <QSignalSpy>
#include <QVector3D>
#include <QMatrix4x4>

//Our includes
#include "cwHeadCoupledPerspectiveProjection.h"
#include "cwViewMatrixComposer.h"
#include "cwAbstractHeadTracker.h"
#include "cwCamera.h"
#include "cwProjection.h"
#include "cw3dRegionViewer.h"

// Mock tracker for testing
class MockHeadTrackerCP2 : public cwAbstractHeadTracker
{
    Q_OBJECT

public:
    using cwAbstractHeadTracker::cwAbstractHeadTracker;

    bool isAvailable() const override { return true; }

    using cwAbstractHeadTracker::setRawEyePosition;
    using cwAbstractHeadTracker::setRawHeadRotation;

protected:
    void startTracking() override {}
    void stopTracking() override {}
};

// Testable subclass that exposes protected members
class TestableHeadCoupledProjection : public cwHeadCoupledPerspectiveProjection
{
public:
    using cwHeadCoupledPerspectiveProjection::cwHeadCoupledPerspectiveProjection;
    using cwHeadCoupledPerspectiveProjection::calculateProjection;

    cwProjection getProjection() const { return projection(); }
};

TEST_CASE("cwViewMatrixComposer identity passthrough", "[HeadCoupledProjection]")
{
    cwCamera camera;
    camera.setViewport(QRect(0, 0, 800, 600));

    cwViewMatrixComposer composer;
    composer.setCamera(&camera);

    QMatrix4x4 baseView;
    baseView.translate(0.0f, 0.0f, -50.0f);

    composer.setBaseViewMatrix(baseView);

    // With identity head offset, camera should get the base view matrix
    CHECK(camera.viewMatrix() == baseView);
}

TEST_CASE("cwViewMatrixComposer composes head offset with base", "[HeadCoupledProjection]")
{
    cwCamera camera;
    camera.setViewport(QRect(0, 0, 800, 600));

    cwViewMatrixComposer composer;
    composer.setCamera(&camera);

    QMatrix4x4 baseView;
    baseView.translate(0.0f, 0.0f, -50.0f);
    composer.setBaseViewMatrix(baseView);

    QMatrix4x4 headOffset;
    headOffset.translate(-0.1f, -0.05f, 0.0f);
    composer.setHeadOffset(headOffset);

    QMatrix4x4 expected = headOffset * baseView;
    CHECK(camera.viewMatrix() == expected);
}

TEST_CASE("cwViewMatrixComposer base view matrix is stored independently", "[HeadCoupledProjection]")
{
    cwCamera camera;
    camera.setViewport(QRect(0, 0, 800, 600));

    cwViewMatrixComposer composer;
    composer.setCamera(&camera);

    QMatrix4x4 baseView;
    baseView.translate(0.0f, 0.0f, -50.0f);
    composer.setBaseViewMatrix(baseView);

    // Apply head offset
    QMatrix4x4 headOffset;
    headOffset.translate(-0.1f, 0.0f, 0.0f);
    composer.setHeadOffset(headOffset);

    // baseViewMatrix() should still return the original base, not the composed result
    CHECK(composer.baseViewMatrix() == baseView);
}

TEST_CASE("cwHeadCoupledPerspectiveProjection centered eye produces symmetric frustum", "[HeadCoupledProjection]")
{
    cw3dRegionViewer viewer;
    viewer.setWidth(800);
    viewer.setHeight(600);

    cwViewMatrixComposer composer;
    composer.setCamera(viewer.camera());

    MockHeadTrackerCP2 tracker;
    tracker.setSmoothing(0.0);

    TestableHeadCoupledProjection proj;
    proj.setViewer(&viewer);
    proj.setTracker(&tracker);
    proj.setViewMatrixComposer(&composer);
    proj.setFieldOfView(55.0);
    proj.setScreenWidthMeters(0.30);
    proj.setScreenHeightMeters(0.19);
    proj.setEnabled(true);

    // Centered eye at default distance — frustum should be symmetric
    cwProjection result = proj.getProjection();

    // Check symmetry: left should be -right, bottom should be -top
    CHECK_THAT(result.left() + result.right(), Catch::Matchers::WithinAbs(0.0, 1e-6));
    CHECK_THAT(result.bottom() + result.top(), Catch::Matchers::WithinAbs(0.0, 1e-6));
}

TEST_CASE("cwHeadCoupledPerspectiveProjection off-center eye produces asymmetric frustum", "[HeadCoupledProjection]")
{
    cw3dRegionViewer viewer;
    viewer.setWidth(800);
    viewer.setHeight(600);

    cwViewMatrixComposer composer;
    composer.setCamera(viewer.camera());

    MockHeadTrackerCP2 tracker;
    tracker.setSmoothing(0.0);
    tracker.start();

    TestableHeadCoupledProjection proj;
    proj.setViewer(&viewer);
    proj.setTracker(&tracker);
    proj.setViewMatrixComposer(&composer);
    proj.setFieldOfView(55.0);
    proj.setScreenWidthMeters(0.30);
    proj.setScreenHeightMeters(0.19);
    proj.setEnabled(true);

    // Move eye to the right
    tracker.setRawEyePosition(QVector3D(0.1f, 0.0f, 0.5f));

    cwProjection result = proj.getProjection();

    // When eye moves right, left side of frustum should be larger (more negative)
    // and right side should be smaller
    CHECK(result.left() < -std::abs(result.right()));
}

TEST_CASE("cwHeadCoupledPerspectiveProjection fieldOfView and aspectRatio are stored", "[HeadCoupledProjection]")
{
    cw3dRegionViewer viewer;
    viewer.setWidth(800);
    viewer.setHeight(600);

    cwViewMatrixComposer composer;
    composer.setCamera(viewer.camera());

    MockHeadTrackerCP2 tracker;
    tracker.setSmoothing(0.0);

    TestableHeadCoupledProjection proj;
    proj.setViewer(&viewer);
    proj.setTracker(&tracker);
    proj.setViewMatrixComposer(&composer);
    proj.setFieldOfView(55.0);
    proj.setScreenWidthMeters(0.30);
    proj.setScreenHeightMeters(0.19);
    proj.setEnabled(true);

    cwProjection result = proj.getProjection();

    // FOV and aspect ratio should be populated (not zero)
    CHECK(result.fieldOfView() > 0.0);
    CHECK(result.aspectRatio() > 0.0);
}

TEST_CASE("cwHeadCoupledPerspectiveProjection sensitivity scales offset", "[HeadCoupledProjection]")
{
    cw3dRegionViewer viewer;
    viewer.setWidth(800);
    viewer.setHeight(600);

    cwViewMatrixComposer composer;
    composer.setCamera(viewer.camera());

    MockHeadTrackerCP2 tracker;
    tracker.setSmoothing(0.0);
    tracker.start();

    TestableHeadCoupledProjection proj;
    proj.setViewer(&viewer);
    proj.setTracker(&tracker);
    proj.setViewMatrixComposer(&composer);
    proj.setFieldOfView(55.0);
    proj.setScreenWidthMeters(0.30);
    proj.setScreenHeightMeters(0.19);
    proj.setSensitivity(1.0);
    proj.setEnabled(true);

    tracker.setRawEyePosition(QVector3D(0.1f, 0.0f, 0.5f));
    cwProjection result1 = proj.getProjection();
    double asymmetry1 = result1.right() + result1.left(); // how far from symmetric

    // Increase sensitivity
    proj.setSensitivity(2.0);
    cwProjection result2 = proj.getProjection();
    double asymmetry2 = result2.right() + result2.left();

    // Higher sensitivity should produce more asymmetry
    CHECK(std::abs(asymmetry2) > std::abs(asymmetry1));
}

#include "HeadCoupledProjectionTest.moc"
