//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwColorBy.h"
#include "cwColorByUniform.h"
#include "cwColorRamp.h"
#include "cwRenderLinePlot.h"
#include "cwRenderTexturedItems.h"

//Qt includes
#include <QCoreApplication>
#include <QSettings>
#include <QVector3D>

//Std includes
#include <cstring>

using namespace Catch;

namespace {

constexpr float kDeepest = -500.0f;
constexpr float kHighest = 100.0f;
constexpr float kFlatZ = -42.0f;
constexpr double kMovedPivot = -200.0;
constexpr double kMovedHalfExtent = 300.0;
constexpr double kStoredPivot = -75.5;
constexpr double kStoredScrapStrength = 0.25;
constexpr float kNarrowLow = -37.0f;
constexpr float kNarrowHigh = 12.0f;
constexpr double kTickMargin = 1e-6;

// Points every cwColorBy in the test at its own QSettings store, unique to this
// process so parallel runs never collide, and clears it on both ends.
class ScopedColorBySettings
{
public:
    ScopedColorBySettings() :
        m_previousApplicationName(QCoreApplication::applicationName())
    {
        QCoreApplication::setApplicationName(
            QStringLiteral("cavewhere-test-colorBy-%1").arg(QCoreApplication::applicationPid()));
        QSettings().clear();
    }

    ~ScopedColorBySettings()
    {
        QSettings().clear();
        QCoreApplication::setApplicationName(m_previousApplicationName);
    }

private:
    QString m_previousApplicationName;
};

// Two shots spanning kDeepest..kHighest in z
QVector<QVector3D> deepToHighGeometry()
{
    return {
        QVector3D(0.0f, 0.0f, kHighest),
        QVector3D(10.0f, 0.0f, 0.0f),
        QVector3D(10.0f, 0.0f, 0.0f),
        QVector3D(20.0f, 0.0f, kDeepest)
    };
}

int modeOf(const cwColorBy& colorBy)
{
    return static_cast<int>(colorBy.uniform().params[0]);
}

}

TEST_CASE("cwColorBy is off without targets", "[ColorBy]")
{
    ScopedColorBySettings settings;

    cwColorBy colorBy;
    CHECK(colorBy.source() == cwColorBy::Source::None);
    CHECK(colorBy.rampId() == cwColorRamp::defaultDepthRampId());
    CHECK(modeOf(colorBy) == cwColorByUniform::Off);
    CHECK(colorBy.normalized(0.0) == 0.0);
    CHECK_FALSE(colorBy.rampIds().contains(cwColorRamp::defaultCategoricalRampId()));
    CHECK(colorBy.rampIds().contains(cwColorRamp::defaultDepthRampId()));
}

TEST_CASE("cwColorBy normalizes depth around the pivot", "[ColorBy]")
{
    ScopedColorBySettings settings;

    cwRenderLinePlot linePlot;
    cwRenderTexturedItems items;
    cwColorBy colorBy;
    colorBy.setTargets(&linePlot, &items);

    linePlot.setGeometry(deepToHighGeometry());
    colorBy.setSource(cwColorBy::Source::Depth);
    colorBy.setRampId(cwColorRamp::defaultDepthRampId());
    colorBy.setPivot(0.0);

    CHECK(colorBy.rangeMin() == Approx(kDeepest));
    CHECK(colorBy.rangeMax() == Approx(kHighest));

    SECTION("diverging ramp puts the pivot at the middle") {
        CHECK(colorBy.normalized(kDeepest) == Approx(0.0));
        CHECK(colorBy.normalized(0.0) == Approx(0.5));
        CHECK(colorBy.normalized(kHighest) == Approx(0.6));

        const cwColorByUniform uniform = colorBy.uniform();
        CHECK(uniform.range[0] == Approx(kDeepest));
        CHECK(uniform.range[1] == Approx(kHighest));
        CHECK(uniform.range[2] == Approx(0.0));
        CHECK(uniform.range[3] == Approx(-kDeepest));
        CHECK(modeOf(colorBy) == cwColorByUniform::Diverging);
        CHECK(static_cast<int>(uniform.params[3]) == cwColorByUniform::WorldZ);

        CHECK(linePlot.colorBy() == uniform);
        CHECK(items.sharedUniformBlock() == cwColorByUniform::toBytes(uniform));
    }

    SECTION("sequential ramp spans the whole range") {
        colorBy.setRampId(QStringLiteral("blue"));
        CHECK(modeOf(colorBy) == cwColorByUniform::Sequential);
        CHECK(colorBy.normalized(kDeepest) == Approx(0.0));
        CHECK(colorBy.normalized(kHighest) == Approx(1.0));
        CHECK(linePlot.colorBy() == colorBy.uniform());
    }

    SECTION("moving the pivot keeps it white and resizes the half extent") {
        colorBy.setPivot(kMovedPivot);
        CHECK(colorBy.normalized(kMovedPivot) == Approx(0.5));
        CHECK(colorBy.uniform().range[3] == Approx(kMovedHalfExtent));
    }

    SECTION("source None turns the block off") {
        colorBy.setSource(cwColorBy::Source::None);
        CHECK(modeOf(colorBy) == cwColorByUniform::Off);
        CHECK(linePlot.colorBy() == colorBy.uniform());
    }
}

TEST_CASE("cwColorBy is off for flat geometry", "[ColorBy]")
{
    ScopedColorBySettings settings;

    cwRenderLinePlot linePlot;
    cwColorBy colorBy;
    colorBy.setTargets(&linePlot, nullptr);
    colorBy.setSource(cwColorBy::Source::Depth);

    linePlot.setGeometry({
        QVector3D(0.0f, 0.0f, kFlatZ),
        QVector3D(10.0f, 0.0f, kFlatZ)
    });

    CHECK(colorBy.rangeMin() == Approx(kFlatZ));
    CHECK(colorBy.rangeMax() == Approx(kFlatZ));
    CHECK(modeOf(colorBy) == cwColorByUniform::Off);
    CHECK(linePlot.colorBy() == colorBy.uniform());
}

TEST_CASE("cwColorBy persists its settings", "[ColorBy]")
{
    ScopedColorBySettings settings;

    {
        cwColorBy colorBy;
        colorBy.setSource(cwColorBy::Source::Depth);
        colorBy.setRampId(QStringLiteral("viridis"));
        colorBy.setPivot(kStoredPivot);
        colorBy.setScrapStrength(kStoredScrapStrength);
    }

    cwColorBy reloaded;
    CHECK(reloaded.source() == cwColorBy::Source::Depth);
    CHECK(reloaded.rampId() == QStringLiteral("viridis"));
    CHECK(reloaded.pivot() == Approx(kStoredPivot));
    CHECK(reloaded.scrapStrength() == Approx(kStoredScrapStrength));

    reloaded.setScrapStrength(2.0);
    CHECK(reloaded.scrapStrength() == Approx(1.0));
}

TEST_CASE("cwColorBy snaps an unknown stored ramp to the depth preset", "[ColorBy]")
{
    ScopedColorBySettings settings;

    QSettings().setValue(QStringLiteral("colorBy/rampId"), QStringLiteral("noSuchRamp"));

    cwColorBy colorBy;
    CHECK(colorBy.rampId() == cwColorRamp::defaultDepthRampId());
    CHECK(colorBy.rampIds().contains(colorBy.rampId()));
}

TEST_CASE("cwRenderTexturedItems keeps an item's color value through updateItem", "[ColorBy]")
{
    constexpr float kSlot = 3.0f;
    const auto colorValueOf = [](const cwRenderTexturedItems& items, uint32_t id) {
        float value = 0.0f;
        // x of the item's vec4 is the color-by value.
        std::memcpy(&value, items.item(id).uniformBlock.constData(), sizeof(float));
        return value;
    };

    cwRenderTexturedItems items;
    cwRenderTexturedItems::Item item;
    item.uniformBlock = cwRenderTexturedItems::emptyItemBlock();
    const uint32_t id = items.addItem(item);
    items.setItemColorValue(id, kSlot);
    REQUIRE(colorValueOf(items, id) == kSlot);

    // A re-triangulated LiDAR note arrives with a fresh, zeroed block.
    items.updateItem(id, item);
    CHECK(colorValueOf(items, id) == kSlot);
    CHECK(items.item(id).uniformBlock.size() == cwRenderTexturedItems::emptyItemBlock().size());
}

TEST_CASE("cwColorBy rounds legend ticks like the scale bar", "[ColorBy]")
{
    ScopedColorBySettings settings;

    const auto spanGeometry = [](float lowZ, float highZ) {
        return QVector<QVector3D>{
            QVector3D(0.0f, 0.0f, highZ),
            QVector3D(10.0f, 0.0f, lowZ)
        };
    };

    const auto checkTicks = [](const QList<double>& ticks, const QList<double>& expected) {
        REQUIRE(ticks.size() == expected.size());
        for (int i = 0; i < expected.size(); ++i) {
            CHECK(ticks.at(i) == Approx(expected.at(i)).margin(kTickMargin));
        }
    };

    cwRenderLinePlot linePlot;
    cwColorBy colorBy;
    colorBy.setTargets(&linePlot, nullptr);
    colorBy.setSource(cwColorBy::Source::Depth);

    SECTION("metric -500..100 m steps by 100 m") {
        linePlot.setGeometry(spanGeometry(kDeepest, kHighest));
        checkTicks(colorBy.legendTicks(), {-500.0, -400.0, -300.0, -200.0, -100.0, 0.0, 100.0});
        CHECK(colorBy.formatDepth(-500.0) == QStringLiteral("-500 m"));
    }

    SECTION("metric -37..12 m steps by 10 m") {
        linePlot.setGeometry(spanGeometry(kNarrowLow, kNarrowHigh));
        checkTicks(colorBy.legendTicks(), {-30.0, -20.0, -10.0, 0.0, 10.0});
    }

    SECTION("imperial -500..100 m steps by 500 ft, returned in meters") {
        constexpr double kMetersPerFoot = 0.3048;
        linePlot.setGeometry(spanGeometry(kDeepest, kHighest));
        colorBy.setUnitSystem(cwUnits::Imperial);
        checkTicks(colorBy.legendTicks(), {-1500.0 * kMetersPerFoot,
                                           -1000.0 * kMetersPerFoot,
                                           -500.0 * kMetersPerFoot,
                                           0.0});
        CHECK(colorBy.formatDepth(colorBy.legendTicks().first()) == QStringLiteral("-1500 ft"));
        CHECK(colorBy.formatDepth(-0.1) == QStringLiteral("0 ft"));
    }

    SECTION("a flat range gets one tick") {
        linePlot.setGeometry(spanGeometry(kFlatZ, kFlatZ));
        checkTicks(colorBy.legendTicks(), {kFlatZ});
    }
}
