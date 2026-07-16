// Regression tests for issue #534: the grid plane used to sit at a hardcoded
// elevation (z = -75) regardless of the survey. It should instead snap to the
// lowest depth of the line plot. cwRegionSceneManager owns both the render line
// plot and the grid plane and wires them together, so the behaviour is tested
// through its public surface: feed geometry into the line plot and read back the
// grid plane's origin. CPU-only — no RHI or GPU is touched.

#include <catch2/catch_test_macros.hpp>

#include <QPlane3D>
#include <QVector3D>

#include "cwRegionSceneManager.h"
#include "cwRenderLinePlot.h"
#include "cwRenderGridPlane.h"
#include "cwSignalSpy.h"

TEST_CASE("cwRegionSceneManager snaps the grid plane to the lowest cave depth",
          "[cwRegionSceneManager]")
{
    cwRegionSceneManager sceneManager;
    cwRenderLinePlot* linePlot = sceneManager.linePlot();
    cwRenderGridPlane* gridPlane = sceneManager.gridPlane();

    REQUIRE(linePlot != nullptr);
    REQUIRE(gridPlane != nullptr);

    SECTION("default elevation is the world origin before any geometry") {
        CHECK(gridPlane->plane().origin().z() == 0.0f);
        CHECK(gridPlane->plane().normal() == QVector3D(0.0f, 0.0f, 1.0f));
    }

    SECTION("grid drops to the minimum z of the line plot") {
        cwSignalSpy planeSpy(gridPlane, &cwRenderGridPlane::planeChanged);

        const float lowest = -123.5f;
        linePlot->setGeometry({
            QVector3D(10.0f, 20.0f, 30.0f),
            QVector3D(0.0f, 0.0f, lowest),
            QVector3D(5.0f, 5.0f, -50.0f)
        });

        CHECK(planeSpy.count() >= 1);
        CHECK(gridPlane->plane().origin().z() == lowest);
        // Only the elevation tracks the survey; the plane stays horizontal and
        // centred on the world origin in x/y.
        CHECK(gridPlane->plane().origin().x() == 0.0f);
        CHECK(gridPlane->plane().origin().y() == 0.0f);
        CHECK(gridPlane->plane().normal() == QVector3D(0.0f, 0.0f, 1.0f));
    }

    SECTION("empty geometry leaves the grid at its default") {
        linePlot->setGeometry({});

        // The empty-geometry min-z sentinel (+FLT_MAX) must never reach the grid.
        CHECK(gridPlane->plane().origin().z() == 0.0f);
    }

    SECTION("setGeometry emits geometryChanged") {
        cwSignalSpy geometrySpy(linePlot, &cwRenderLinePlot::geometryChanged);
        linePlot->setGeometry({ QVector3D(0.0f, 0.0f, -10.0f) });
        CHECK(geometrySpy.count() == 1);
    }
}
