// Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
using namespace Catch;

// Our includes
#include "cwTriangulateWarping.h"
#include "cwTriangulateWarpingSettings.h"

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

TEST_CASE("cwTriangulateWarpingSettings persists warping settings", "[cwTriangulateWarpingSettings]")
{
    clearWarpingSettings();

    cwTriangulateWarping warping;
    cwTriangulateWarpingSettings settingsStore(&warping);

    warping.setGridResolutionMeters(4.0);
    warping.setMaxClosestStations(15);
    warping.setShotInterpolationSpacingMeters(0.75);
    warping.setSmoothingRadiusMeters(6.0);
    warping.setUseShotInterpolationSpacing(false);
    warping.setUseMaxClosestStations(true);
    warping.setUseSmoothingRadius(true);

    QSettings settings;
    settings.beginGroup(QStringLiteral("TriangulateWarping"));
    REQUIRE(settings.value(QStringLiteral("gridResolutionMeters")).toDouble() == Approx(4.0));
    REQUIRE(settings.value(QStringLiteral("maxClosestStations")).toInt() == 15);
    REQUIRE(settings.value(QStringLiteral("shotInterpolationSpacingMeters")).toDouble() == Approx(0.75));
    REQUIRE(settings.value(QStringLiteral("smoothingRadiusMeters")).toDouble() == Approx(6.0));
    REQUIRE(settings.value(QStringLiteral("useShotInterpolationSpacing")).toBool() == false);
    REQUIRE(settings.value(QStringLiteral("useMaxClosestStations")).toBool() == true);
    REQUIRE(settings.value(QStringLiteral("useSmoothingRadius")).toBool() == true);
    settings.endGroup();

    SECTION("loading applies stored values to new instance")
    {
        cwTriangulateWarping reloaded;
        cwTriangulateWarpingSettings reloadedStore(&reloaded);

        CHECK(reloaded.gridResolutionMeters() == Approx(4.0));
        CHECK(reloaded.maxClosestStations() == 15);
        CHECK(reloaded.shotInterpolationSpacingMeters() == Approx(0.75));
        CHECK(reloaded.smoothingRadiusMeters() == Approx(6.0));
        CHECK(reloaded.useShotInterpolationSpacing() == false);
        CHECK(reloaded.useMaxClosestStations() == true);
        CHECK(reloaded.useSmoothingRadius() == true);
    }

    clearWarpingSettings();
}
