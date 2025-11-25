// Catch includes
#include <catch2/catch_test_macros.hpp>

// Our includes
#include "cwTriangulateWarping.h"
#include "SignalSpyChecker.h"

// Qt includes
#include <QSettings>

namespace {
void clearWarpingSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("TriangulateWarping"));
    settings.remove(QString());
    settings.endGroup();
}
}

TEST_CASE("cwTriangulateWarping emits signals and resets to defaults", "[cwTriangulateWarping]")
{
    clearWarpingSettings();

    cwTriangulateWarping warping;

    using SignalSpyChecker::SignalSpy;
    SignalSpy gridSpy(&warping, &cwTriangulateWarping::gridResolutionMetersChanged);
    SignalSpy maxClosestSpy(&warping, &cwTriangulateWarping::maxClosestStationsChanged);
    SignalSpy shotSpacingSpy(&warping, &cwTriangulateWarping::shotInterpolationSpacingMetersChanged);
    SignalSpy smoothingSpy(&warping, &cwTriangulateWarping::smoothingRadiusMetersChanged);
    SignalSpy useShotSpy(&warping, &cwTriangulateWarping::useShotInterpolationSpacingChanged);
    SignalSpy useMaxSpy(&warping, &cwTriangulateWarping::useMaxClosestStationsChanged);
    SignalSpy useSmoothSpy(&warping, &cwTriangulateWarping::useSmoothingRadiusChanged);

    gridSpy.setObjectName("gridSpy");
    maxClosestSpy.setObjectName("maxClosestSpy");
    shotSpacingSpy.setObjectName("shotSpacingSpy");
    smoothingSpy.setObjectName("smoothingSpy");
    useShotSpy.setObjectName("useShotSpy");
    useMaxSpy.setObjectName("useMaxSpy");
    useSmoothSpy.setObjectName("useSmoothSpy");

    SignalSpyChecker::Constant checker = {
        { &gridSpy, 0 },
        { &maxClosestSpy, 0 },
        { &shotSpacingSpy, 0 },
        { &smoothingSpy, 0 },
        { &useShotSpy, 0 },
        { &useMaxSpy, 0 },
        { &useSmoothSpy, 0 }
    };

    warping.setGridResolutionMeters(5.0);
    checker[&gridSpy]++;

    warping.setMaxClosestStations(42);
    checker[&maxClosestSpy]++;

    warping.setShotInterpolationSpacingMeters(2.0);
    checker[&shotSpacingSpy]++;

    warping.setSmoothingRadiusMeters(7.5);
    checker[&smoothingSpy]++;

    warping.setUseShotInterpolationSpacing(false);
    checker[&useShotSpy]++;

    warping.setUseMaxClosestStations(false);
    checker[&useMaxSpy]++;

    warping.setUseSmoothingRadius(false);
    checker[&useSmoothSpy]++;

    SECTION("no duplicate signals when setting same value")
    {
        warping.setGridResolutionMeters(5.0);
        warping.setMaxClosestStations(42);
        warping.setShotInterpolationSpacingMeters(2.0);
        warping.setSmoothingRadiusMeters(7.5);
        warping.setUseShotInterpolationSpacing(false);
        warping.setUseMaxClosestStations(false);
        warping.setUseSmoothingRadius(false);

        checker.requireSpies();
    }

    SECTION("reset to defaults resets values and emits if changed")
    {
        checker.clearSpyCounts();
        cwTriangulateWarpingData defaults;

        warping.resetToDefaults();

        CHECK(warping.gridResolutionMeters() == defaults.gridResolutionMeters);
        CHECK(warping.maxClosestStations() == defaults.maxClosestStations);
        CHECK(warping.shotInterpolationSpacingMeters() == defaults.shotInterpolationSpacingMeters);
        CHECK(warping.smoothingRadiusMeters() == defaults.smoothingRadiusMeters);
        CHECK(warping.useShotInterpolationSpacing() == defaults.useShotInterpolationSpacing);
        CHECK(warping.useMaxClosestStations() == defaults.useMaxClosestStations);
        CHECK(warping.useSmoothingRadius() == defaults.useSmoothingRadius);

        checker[&gridSpy]++;
        checker[&maxClosestSpy]++;
        checker[&shotSpacingSpy]++;
        checker[&smoothingSpy]++;
        checker[&useShotSpy]++;
        checker[&useMaxSpy]++;
        checker[&useSmoothSpy]++;
        checker.requireSpies();
    }
}
