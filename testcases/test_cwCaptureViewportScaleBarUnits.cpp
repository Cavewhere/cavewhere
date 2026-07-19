/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCaptureViewport.h"
#include "cwScaleBarItem.h"
#include "cwRegionSceneManager.h"
#include "cwCavingRegion.h"
#include "cwUnits.h"
#include "cwSignalSpy.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>

// The export/print scale bar (#470) must be able to draw in a chosen unit
// system: follow the project by default, or pin this one map to Metric/Imperial.
// It must also refresh live when the project's unit system changes (R3), since
// the project unit system is now user-editable.
TEST_CASE("cwCaptureViewport export scale bar resolves its unit system",
          "[cwCaptureViewport][UnitSystem]")
{
    cwCaptureViewport viewport;
    cwScaleBarItem* bar = viewport.scaleBarItem();
    REQUIRE(bar != nullptr);

    SECTION("defaults to following the project, metric with no project") {
        CHECK(viewport.scaleBarUnitMode() == cwCaptureViewport::FollowProject);
        CHECK(viewport.effectiveScaleBarUnitSystem() == cwUnits::Metric);
        CHECK(bar->unitSystem() == cwUnits::Metric);
    }

    SECTION("a forced mode pins the bar regardless of any project") {
        cwSignalSpy modeSpy(&viewport, &cwCaptureViewport::scaleBarUnitModeChanged);

        viewport.setScaleBarUnitMode(cwCaptureViewport::ForceImperial);
        CHECK(modeSpy.count() == 1);
        CHECK(viewport.effectiveScaleBarUnitSystem() == cwUnits::Imperial);
        CHECK(bar->unitSystem() == cwUnits::Imperial);

        viewport.setScaleBarUnitMode(cwCaptureViewport::ForceMetric);
        CHECK(viewport.effectiveScaleBarUnitSystem() == cwUnits::Metric);
        CHECK(bar->unitSystem() == cwUnits::Metric);
    }

    SECTION("setting the same mode does not re-emit") {
        cwSignalSpy modeSpy(&viewport, &cwCaptureViewport::scaleBarUnitModeChanged);
        viewport.setScaleBarUnitMode(cwCaptureViewport::FollowProject);
        CHECK(modeSpy.count() == 0);
    }
}

TEST_CASE("cwCaptureViewport export scale bar follows the project unit system live",
          "[cwCaptureViewport][UnitSystem]")
{
    cwCaptureViewport viewport;
    cwScaleBarItem* bar = viewport.scaleBarItem();
    REQUIRE(bar != nullptr);

    cwRegionSceneManager sceneManager;
    cwCavingRegion region;
    region.setUnitSystem(cwUnits::Imperial);
    sceneManager.setCavingRegion(&region);

    viewport.setSceneManager(&sceneManager);

    SECTION("adopts the project's system when the scene manager is set") {
        CHECK(viewport.effectiveScaleBarUnitSystem() == cwUnits::Imperial);
        CHECK(bar->unitSystem() == cwUnits::Imperial);
    }

    SECTION("a later project change refreshes the bar live") {
        region.setUnitSystem(cwUnits::Metric);
        CHECK(viewport.effectiveScaleBarUnitSystem() == cwUnits::Metric);
        CHECK(bar->unitSystem() == cwUnits::Metric);
    }

    SECTION("a forced mode ignores the project and its later changes") {
        viewport.setScaleBarUnitMode(cwCaptureViewport::ForceMetric);
        CHECK(bar->unitSystem() == cwUnits::Metric);

        region.setUnitSystem(cwUnits::Imperial);
        // The forced choice wins; the live project change must not move it.
        CHECK(viewport.effectiveScaleBarUnitSystem() == cwUnits::Metric);
        CHECK(bar->unitSystem() == cwUnits::Metric);
    }
}
