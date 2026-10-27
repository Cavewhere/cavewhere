//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwColorBy.h"
#include "cwColorByUniform.h"
#include "cwColorRamp.h"
#include "cwRenderLinePlot.h"
#include "cwRenderTexturedItems.h"
#include "cwScene.h"
#include "cwGeometryItersecter.h"
#include "cwGeometry.h"
#include "TestHelper.h"

//Qt includes
#include <QCoreApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QVector2D>
#include <QVector3D>

//Std includes
#include <cmath>
#include <cstring>

using namespace Catch;

namespace {

constexpr float kDeepest = -500.0f;
constexpr float kHighest = 100.0f;
constexpr float kFlatZ = 3.4f;
constexpr double kFlatSnappedLow = 3.0;
constexpr double kFlatSnappedHigh = 4.0;
constexpr float kWholeFlatZ = 4.0f;
constexpr double kWholeFlatSnappedLow = 3.0;
constexpr double kWholeFlatSnappedHigh = 5.0;
constexpr double kMidpointPivot = -200.0;
constexpr double kMovedPivot = -300.0;
constexpr double kMovedHalfExtent = 400.0;
constexpr double kStoredPivot = -75.5;
constexpr double kStoredScrapStrength = 0.25;
constexpr float kNarrowLow = -37.0f;
constexpr float kNarrowHigh = 12.0f;
constexpr double kTickMargin = 1e-6;
constexpr double kMetersPerFoot = 0.3048;

// Visible-bounds scene: a centerline of three shots at z 10 -> 0 -> -100 -> -400
// and a scrap triangle reaching up to z +30
constexpr float kShotTopZ = 10.0f;
constexpr float kShotMidZ = 0.0f;
constexpr float kShotLowZ = -100.0f;
constexpr float kShotDeepZ = -400.0f;
constexpr float kScrapLowZ = 0.0f;
constexpr float kScrapHighZ = 30.0f;
constexpr float kScrapExtent = 10.0f;
// The visible extents above, snapped outward to their tick intervals
constexpr double kAllSnappedLow = -400.0;
constexpr double kAllSnappedHigh = 100.0;
constexpr double kNoDeepSnappedHigh = 50.0;
constexpr double kShotsOnlySnappedHigh = 20.0;
// Line-list vertices of the shot reaching kShotDeepZ
constexpr int kDeepShotFirstVertex = 4;
constexpr int kVerticesPerShot = 2;
// Loop error: the bar is fixed at 0..3 sigma
constexpr double kGoodSigma = 1.0;
constexpr double kOkaySigma = 2.0;
constexpr double kBadSigma = 3.0;
constexpr double kMidSigma = 1.5;
constexpr float kLoopLegSigma = 0.8f;
// LinePlot.frag's kNoDataColor
constexpr double kNoDataGray = 0.55;

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

// A line plot and textured items in a real scene, with cwColorBy pushing to
// both and following the scene's visible bounds.
struct ColorByScene
{
    ColorByScene()
    {
        linePlot.setScene(&scene);
        items.setScene(&scene);
        colorBy.setTargets(&linePlot, &items);
        colorBy.setScene(&scene);
    }

    // Publishes the geometry, lets the pick structure build, then runs the
    // queued range update.
    void setGeometryAndSettle(const QVector<QVector3D>& geometry)
    {
        linePlot.setGeometry(geometry);
        scene.geometryItersecter()->waitForFinish();
        REQUIRE(waitForQueuedEvents());
    }

    cwScene scene;
    cwRenderLinePlot linePlot;
    cwRenderTexturedItems items;
    cwColorBy colorBy;
};

QVector<QVector3D> threeShotGeometry()
{
    return {
        QVector3D(0.0f, 0.0f, kShotTopZ),
        QVector3D(10.0f, 0.0f, kShotMidZ),
        QVector3D(10.0f, 0.0f, kShotMidZ),
        QVector3D(20.0f, 0.0f, kShotLowZ),
        QVector3D(20.0f, 0.0f, kShotLowZ),
        QVector3D(30.0f, 0.0f, kShotDeepZ)
    };
}

// One triangle spanning kScrapLowZ..kScrapHighZ
cwGeometry scrapTriangle()
{
    cwGeometry geometry({
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 },
        { cwGeometry::Semantic::TexCoord0, cwGeometry::AttributeFormat::Vec2 }
    });

    const QVector<QVector3D> positions = {
        QVector3D(-kScrapExtent, -kScrapExtent, kScrapLowZ),
        QVector3D(kScrapExtent, -kScrapExtent, kScrapLowZ),
        QVector3D(0.0f, kScrapExtent, kScrapHighZ),
    };
    geometry.resizeVertices(static_cast<int>(positions.size()));
    const auto* positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
    const auto* texCoordAttribute = geometry.attribute(cwGeometry::Semantic::TexCoord0);
    for (int i = 0; i < positions.size(); ++i) {
        geometry.set(positionAttribute, i, positions.at(i));
        geometry.set(texCoordAttribute, i, QVector2D(0.0f, 0.0f));
    }

    geometry.setIndices({0u, 1u, 2u});
    geometry.setType(cwGeometry::Type::Triangles);
    return geometry;
}

int modeOf(const cwColorBy& colorBy)
{
    return static_cast<int>(colorBy.uniform().params[cwColorByUniform::ParamMode]);
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

    ColorByScene fixture;
    cwRenderLinePlot& linePlot = fixture.linePlot;
    cwColorBy& colorBy = fixture.colorBy;
    cwRenderTexturedItems& items = fixture.items;

    fixture.setGeometryAndSettle(deepToHighGeometry());
    colorBy.setSource(cwColorBy::Source::Depth);
    colorBy.setRampId(cwColorRamp::defaultDepthRampId());
    colorBy.setPivotManual(0.0);

    CHECK(colorBy.rangeMin() == Approx(kDeepest));
    CHECK(colorBy.rangeMax() == Approx(kHighest));

    SECTION("diverging ramp puts the pivot at the middle") {
        CHECK(colorBy.normalized(kDeepest) == Approx(0.0));
        CHECK(colorBy.normalized(0.0) == Approx(0.5));
        CHECK(colorBy.normalized(kHighest) == Approx(0.6));

        const cwColorByUniform uniform = colorBy.uniform();
        CHECK(uniform.range[cwColorByUniform::RangeMin] == Approx(kDeepest));
        CHECK(uniform.range[cwColorByUniform::RangeMax] == Approx(kHighest));
        CHECK(uniform.range[cwColorByUniform::RangePivot] == Approx(0.0));
        CHECK(uniform.range[cwColorByUniform::RangeHalfExtent] == Approx(-kDeepest));
        CHECK(modeOf(colorBy) == cwColorByUniform::Diverging);
        CHECK(static_cast<int>(uniform.params[cwColorByUniform::ParamValueSource]) == cwColorByUniform::WorldZ);

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
        colorBy.setPivotManual(kMovedPivot);
        CHECK(colorBy.normalized(kMovedPivot) == Approx(0.5));
        CHECK(colorBy.uniform().range[cwColorByUniform::RangeHalfExtent] == Approx(kMovedHalfExtent));
    }

    SECTION("source None turns the block off") {
        colorBy.setSource(cwColorBy::Source::None);
        CHECK(modeOf(colorBy) == cwColorByUniform::Off);
        CHECK(linePlot.colorBy() == colorBy.uniform());
    }
}

TEST_CASE("cwColorBy colors loop error on a fixed sigma scale", "[ColorBy]")
{
    ScopedColorBySettings settings;

    ColorByScene fixture;
    cwRenderLinePlot& linePlot = fixture.linePlot;
    cwColorBy& colorBy = fixture.colorBy;
    cwRenderTexturedItems& items = fixture.items;

    fixture.setGeometryAndSettle(deepToHighGeometry());
    colorBy.setScrapStrength(kStoredScrapStrength);
    colorBy.setSource(cwColorBy::Source::LoopError);

    const cwColorByUniform uniform = colorBy.uniform();
    CHECK(modeOf(colorBy) == cwColorByUniform::Sequential);
    CHECK(static_cast<int>(uniform.params[cwColorByUniform::ParamValueSource]) == cwColorByUniform::PerSegmentError);
    CHECK(uniform.range[cwColorByUniform::RangeMin] == Approx(0.0));
    CHECK(uniform.range[cwColorByUniform::RangeMax] == Approx(kBadSigma));
    CHECK(uniform.range[cwColorByUniform::RangePivot] == Approx(kMidSigma));
    CHECK(uniform.range[cwColorByUniform::RangeHalfExtent] == Approx(kMidSigma));

    // Scraps have no error, so the pushed strength is 0 while the stored one stays
    CHECK(uniform.params[cwColorByUniform::ParamStrength] == 0.0f);
    CHECK(colorBy.scrapStrength() == Approx(kStoredScrapStrength));

    CHECK(linePlot.colorBy() == uniform);
    CHECK(items.sharedUniformBlock() == cwColorByUniform::toBytes(uniform));

    // The ramp the shader samples is the loop error ramp: dark green at 0,
    // lighter green at 1 sigma, dark red at the top
    const cwColorRamp loopRamp = cwColorRamp::loopErrorRamp();
    const QColor first = loopRamp.colorAt(0.0);
    CHECK(uniform.ramp[0][0] == Approx(first.redF()));
    CHECK(uniform.ramp[0][1] == Approx(first.greenF()));
    CHECK(uniform.ramp[0][2] == Approx(first.blueF()));
    CHECK(first.name() == QStringLiteral("#1a9850"));
    CHECK(loopRamp.colorAt(1.0).name() == QStringLiteral("#a50026"));
    CHECK(loopRamp.colorAt(kGoodSigma / kBadSigma).lightnessF() > first.lightnessF());
    CHECK_FALSE(colorBy.rampIds().contains(loopRamp.id()));
    CHECK_FALSE(cwColorRamp::preset(loopRamp.id()).isValid());

    SECTION("returning to depth restores the stored scrap strength") {
        colorBy.setSource(cwColorBy::Source::Depth);
        CHECK(colorBy.uniform().params[cwColorByUniform::ParamStrength] == Approx(kStoredScrapStrength));
        CHECK(static_cast<int>(colorBy.uniform().params[cwColorByUniform::ParamValueSource]) == cwColorByUniform::WorldZ);
    }
}

TEST_CASE("cwColorBy labels the loop error bar in sigma", "[ColorBy]")
{
    ScopedColorBySettings settings;

    ColorByScene fixture;
    cwColorBy& colorBy = fixture.colorBy;
    colorBy.setSource(cwColorBy::Source::Depth);
    fixture.setGeometryAndSettle(deepToHighGeometry());
    REQUIRE(colorBy.rangeMin() == Approx(kDeepest));
    REQUIRE(colorBy.rangeMax() == Approx(kHighest));
    CHECK(colorBy.bandName(kGoodSigma) == QString());

    QSignalSpy rangeSpy(&colorBy, &cwColorBy::rangeChanged);
    QSignalSpy ticksSpy(&colorBy, &cwColorBy::legendTicksChanged);
    colorBy.setSource(cwColorBy::Source::LoopError);
    CHECK(rangeSpy.count() == 1);
    CHECK(ticksSpy.count() == 1);

    CHECK(colorBy.rangeMin() == Approx(0.0));
    CHECK(colorBy.rangeMax() == Approx(kBadSigma));
    CHECK(colorBy.legendTicks() == QList<double>{0.0, kGoodSigma, kOkaySigma, kBadSigma});
    CHECK(colorBy.normalized(kMidSigma) == Approx(0.5));
    CHECK(colorBy.normalized(kBadSigma * 2.0) == Approx(1.0));

    CHECK(colorBy.formatTick(0.0) == QStringLiteral(u"0 \u03c3"));
    CHECK(colorBy.formatTick(kGoodSigma) == QStringLiteral(u"1 \u03c3"));
    CHECK(colorBy.formatTick(kOkaySigma) == QStringLiteral(u"2 \u03c3"));
    CHECK(colorBy.formatTick(kBadSigma) == QStringLiteral(u"\u22653 \u03c3"));

    CHECK(colorBy.bandName(0.0) == QStringLiteral("Perfect"));
    CHECK(colorBy.bandName(kLoopLegSigma) == QStringLiteral("Perfect"));
    CHECK(colorBy.bandName(kGoodSigma) == QStringLiteral("Good"));
    CHECK(colorBy.bandName(kMidSigma) == QStringLiteral("Good"));
    CHECK(colorBy.bandName(kOkaySigma) == QStringLiteral("Okay"));
    CHECK(colorBy.bandName(kBadSigma) == QStringLiteral("Bad"));
    CHECK(colorBy.bandName(kBadSigma * 3.0) == QStringLiteral("Bad"));

    CHECK(colorBy.noDataColor() == QColor::fromRgbF(kNoDataGray, kNoDataGray, kNoDataGray));

    SECTION("switching back to depth restores the snapped depth range") {
        rangeSpy.clear();
        ticksSpy.clear();
        colorBy.setSource(cwColorBy::Source::Depth);
        CHECK(rangeSpy.count() == 1);
        CHECK(ticksSpy.count() == 1);
        CHECK(colorBy.rangeMin() == Approx(kDeepest));
        CHECK(colorBy.rangeMax() == Approx(kHighest));
        CHECK(colorBy.legendTicks().first() == Approx(kDeepest));
        CHECK(colorBy.legendTicks().last() == Approx(kHighest));
        CHECK(colorBy.formatTick(kDeepest) == QStringLiteral(u"\u2212500 m"));
        CHECK(colorBy.uniform().range[cwColorByUniform::RangeMin] == Approx(kDeepest));
    }
}

TEST_CASE("cwRenderLinePlot replaces segment errors and new geometry clears them", "[ColorBy]")
{
    cwRenderLinePlot linePlot;
    linePlot.setGeometry(deepToHighGeometry());
    CHECK(linePlot.segmentErrors().isEmpty());

    const QVector<float> errors = {kLoopLegSigma, cwLinePlotGeometry::kNoErrorValue};
    linePlot.setSegmentErrors(errors);
    CHECK(linePlot.segmentErrors() == errors);

    // The color values stay apart from the errors
    CHECK(linePlot.colorValues() == QVector<float>(deepToHighGeometry().size(), 0.0f));

    linePlot.setGeometry(deepToHighGeometry());
    CHECK(linePlot.segmentErrors().isEmpty());
}

TEST_CASE("cwColorBy defaults the pivot to the middle of the range", "[ColorBy]")
{
    ScopedColorBySettings settings;

    ColorByScene fixture;
    cwColorBy& colorBy = fixture.colorBy;
    colorBy.setSource(cwColorBy::Source::Depth);
    colorBy.setRampId(cwColorRamp::defaultDepthRampId());
    fixture.setGeometryAndSettle(deepToHighGeometry());

    CHECK_FALSE(colorBy.hasPivotManual());
    CHECK(colorBy.pivot() == Approx(kMidpointPivot));
    CHECK(colorBy.normalized(kMidpointPivot) == Approx(0.5));
    CHECK(colorBy.uniform().range[cwColorByUniform::RangePivot] == Approx(kMidpointPivot));

    SECTION("a manual pivot wins until it is cleared") {
        QSignalSpy pivotSpy(&colorBy, &cwColorBy::pivotChanged);
        QSignalSpy manualSpy(&colorBy, &cwColorBy::pivotManualChanged);

        colorBy.setPivotManual(0.0);
        CHECK(colorBy.hasPivotManual());
        CHECK(colorBy.pivot() == Approx(0.0));
        CHECK(colorBy.normalized(0.0) == Approx(0.5));
        CHECK(pivotSpy.count() == 1);
        CHECK(manualSpy.count() == 1);

        colorBy.clearPivotManual();
        CHECK_FALSE(colorBy.hasPivotManual());
        CHECK(colorBy.pivot() == Approx(kMidpointPivot));
        CHECK(colorBy.uniform().range[cwColorByUniform::RangePivot] == Approx(kMidpointPivot));
        CHECK(pivotSpy.count() == 2);
        CHECK(manualSpy.count() == 2);
    }

    SECTION("a range change moves the automatic pivot") {
        QSignalSpy pivotSpy(&colorBy, &cwColorBy::pivotChanged);
        fixture.setGeometryAndSettle(threeShotGeometry());

        const double midpoint = (kAllSnappedLow + kAllSnappedHigh) * 0.5;
        CHECK(pivotSpy.count() == 1);
        CHECK(colorBy.pivot() == Approx(midpoint));
        CHECK(colorBy.uniform().range[cwColorByUniform::RangePivot] == Approx(midpoint));
    }

    SECTION("a range change keeps a manual pivot") {
        colorBy.setPivotManual(0.0);
        QSignalSpy pivotSpy(&colorBy, &cwColorBy::pivotChanged);
        fixture.setGeometryAndSettle(threeShotGeometry());

        CHECK(pivotSpy.count() == 0);
        CHECK(colorBy.pivot() == Approx(0.0));
    }
}

TEST_CASE("cwColorBy brackets flat geometry with whole units", "[ColorBy]")
{
    ScopedColorBySettings settings;

    ColorByScene fixture;
    cwRenderLinePlot& linePlot = fixture.linePlot;
    cwColorBy& colorBy = fixture.colorBy;
    colorBy.setSource(cwColorBy::Source::Depth);

    SECTION("fractional data snaps to the unit below and above") {
        fixture.setGeometryAndSettle({
            QVector3D(0.0f, 0.0f, kFlatZ),
            QVector3D(10.0f, 0.0f, kFlatZ)
        });

        CHECK(colorBy.rangeMin() == Approx(kFlatSnappedLow));
        CHECK(colorBy.rangeMax() == Approx(kFlatSnappedHigh));
        REQUIRE(colorBy.legendTicks().size() == 2);
        CHECK(colorBy.legendTicks().at(0) == Approx(kFlatSnappedLow));
        CHECK(colorBy.legendTicks().at(1) == Approx(kFlatSnappedHigh));
        CHECK(std::isfinite(colorBy.normalized(kFlatZ)));
    }

    SECTION("whole-unit data sits in the middle of the bar") {
        fixture.setGeometryAndSettle({
            QVector3D(0.0f, 0.0f, kWholeFlatZ),
            QVector3D(10.0f, 0.0f, kWholeFlatZ)
        });

        CHECK(colorBy.rangeMin() == Approx(kWholeFlatSnappedLow));
        CHECK(colorBy.rangeMax() == Approx(kWholeFlatSnappedHigh));
        REQUIRE(colorBy.legendTicks().size() == 3);
        CHECK(colorBy.legendTicks().at(0) == Approx(kWholeFlatSnappedLow));
        CHECK(colorBy.legendTicks().at(1) == Approx(kWholeFlatZ));
        CHECK(colorBy.legendTicks().at(2) == Approx(kWholeFlatSnappedHigh));
        CHECK(colorBy.normalized(kWholeFlatZ) == Approx(0.5));
    }

    // The default depth ramp is diverging
    CHECK(modeOf(colorBy) == cwColorByUniform::Diverging);
    CHECK(linePlot.colorBy() == colorBy.uniform());
}

TEST_CASE("cwColorBy persists its settings", "[ColorBy]")
{
    ScopedColorBySettings settings;

    {
        cwColorBy colorBy;
        colorBy.setSource(cwColorBy::Source::Depth);
        colorBy.setRampId(QStringLiteral("viridis"));
        colorBy.setPivotManual(kStoredPivot);
        colorBy.setScrapStrength(kStoredScrapStrength);
    }

    // The source and the manual pivot last only for the session
    cwColorBy reloaded;
    CHECK(reloaded.source() == cwColorBy::Source::None);
    CHECK_FALSE(reloaded.hasPivotManual());
    CHECK(reloaded.rampId() == QStringLiteral("viridis"));
    CHECK(reloaded.scrapStrength() == Approx(kStoredScrapStrength));

    reloaded.setScrapStrength(2.0);
    CHECK(reloaded.scrapStrength() == Approx(1.0));
}

TEST_CASE("cwColorBy drops a source and pivot stored by an older build", "[ColorBy]")
{
    ScopedColorBySettings settings;

    const QString sourceKey = QStringLiteral("colorBy/source");
    const QString pivotKey = QStringLiteral("colorBy/pivot");
    QSettings().setValue(sourceKey, static_cast<int>(cwColorBy::Source::Depth));
    QSettings().setValue(pivotKey, kStoredPivot);

    cwColorBy colorBy;
    CHECK(colorBy.source() == cwColorBy::Source::None);
    CHECK_FALSE(colorBy.hasPivotManual());
    CHECK_FALSE(QSettings().contains(sourceKey));
    CHECK_FALSE(QSettings().contains(pivotKey));
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

    ColorByScene fixture;
    cwRenderLinePlot& linePlot = fixture.linePlot;
    cwColorBy& colorBy = fixture.colorBy;
    colorBy.setSource(cwColorBy::Source::Depth);

    SECTION("metric -500..100 m snaps to itself and steps by 100 m") {
        fixture.setGeometryAndSettle(spanGeometry(kDeepest, kHighest));
        CHECK(colorBy.rangeMin() == Approx(kDeepest));
        CHECK(colorBy.rangeMax() == Approx(kHighest));
        checkTicks(colorBy.legendTicks(), {-500.0, -400.0, -300.0, -200.0, -100.0, 0.0, 100.0});
        CHECK(colorBy.formatDepth(-500.0) == QStringLiteral(u"\u2212500 m"));
    }

    SECTION("metric -37..12 m snaps to -40..20 and steps by 10 m") {
        fixture.setGeometryAndSettle(spanGeometry(kNarrowLow, kNarrowHigh));
        CHECK(colorBy.rangeMin() == Approx(-40.0));
        CHECK(colorBy.rangeMax() == Approx(20.0));
        CHECK(colorBy.pivot() == Approx(-10.0));
        checkTicks(colorBy.legendTicks(), {-40.0, -30.0, -20.0, -10.0, 0.0, 10.0, 20.0});
    }

    SECTION("imperial -500..100 m snaps to -2000..500 ft, returned in meters") {
        fixture.setGeometryAndSettle(spanGeometry(kDeepest, kHighest));
        colorBy.setUnitSystem(cwUnits::Imperial);
        checkTicks(colorBy.legendTicks(), {-2000.0 * kMetersPerFoot,
                                           -1500.0 * kMetersPerFoot,
                                           -1000.0 * kMetersPerFoot,
                                           -500.0 * kMetersPerFoot,
                                           0.0,
                                           500.0 * kMetersPerFoot});
        CHECK(colorBy.formatDepth(colorBy.legendTicks().first()) == QStringLiteral(u"\u22122000 ft"));
        CHECK(colorBy.formatDepth(-0.1) == QStringLiteral("0 ft"));
    }

    SECTION("positive 12..37 m snaps to 10..40 with no sign") {
        fixture.setGeometryAndSettle(spanGeometry(kNarrowHigh, -kNarrowLow));
        CHECK(colorBy.rangeMin() == Approx(10.0));
        CHECK(colorBy.rangeMax() == Approx(40.0));
        CHECK(colorBy.legendTicks().first() == Approx(10.0));
        CHECK(colorBy.legendTicks().last() == Approx(40.0));
        CHECK(colorBy.formatDepth(10.0) == QStringLiteral("10 m"));
    }

    SECTION("a data change inside the snapped range emits nothing") {
        fixture.setGeometryAndSettle(spanGeometry(kDeepest, kHighest));
        QSignalSpy rangeSpy(&colorBy, &cwColorBy::rangeChanged);
        fixture.setGeometryAndSettle(spanGeometry(kDeepest + 10.0f, kHighest - 10.0f));
        CHECK(rangeSpy.count() == 0);
        CHECK(colorBy.rangeMin() == Approx(kDeepest));
        CHECK(colorBy.rangeMax() == Approx(kHighest));
    }

    SECTION("switching to imperial re-snaps the range") {
        fixture.setGeometryAndSettle(spanGeometry(kDeepest, kHighest));
        QSignalSpy rangeSpy(&colorBy, &cwColorBy::rangeChanged);
        colorBy.setUnitSystem(cwUnits::Imperial);
        CHECK(rangeSpy.count() == 1);
        CHECK(colorBy.rangeMin() == Approx(-2000.0 * kMetersPerFoot));
        CHECK(colorBy.rangeMax() == Approx(500.0 * kMetersPerFoot));
    }
}

TEST_CASE("cwColorBy depth range follows the visible bounds", "[ColorBy]")
{
    ScopedColorBySettings settings;

    ColorByScene fixture;
    cwRenderLinePlot& linePlot = fixture.linePlot;
    cwColorBy& colorBy = fixture.colorBy;
    cwRenderTexturedItems& items = fixture.items;
    colorBy.setSource(cwColorBy::Source::Depth);

    cwRenderTexturedItems::Item scrap;
    scrap.geometry = scrapTriangle();
    const uint32_t scrapId = items.addItem(scrap);
    fixture.setGeometryAndSettle(threeShotGeometry());

    CHECK(colorBy.rangeMin() == Approx(kAllSnappedLow));
    CHECK(colorBy.rangeMax() == Approx(kAllSnappedHigh));

    linePlot.setRangeVisible(kDeepShotFirstVertex, kVerticesPerShot, false);
    REQUIRE(waitUntil([&colorBy] { return colorBy.rangeMax() == Approx(kNoDeepSnappedHigh); }));
    CHECK(colorBy.rangeMin() == Approx(kShotLowZ));
    CHECK(colorBy.rangeMax() == Approx(kNoDeepSnappedHigh));

    items.setItemVisible(scrapId, false);
    REQUIRE(waitUntil([&colorBy] { return colorBy.rangeMax() == Approx(kShotsOnlySnappedHigh); }));
    CHECK(colorBy.rangeMin() == Approx(kShotLowZ));
    CHECK(colorBy.rangeMax() == Approx(kShotsOnlySnappedHigh));

    // Nothing visible keeps the last range
    const int vertexCount = static_cast<int>(threeShotGeometry().size());
    linePlot.setRangeVisible(0, vertexCount, false);
    REQUIRE(waitForQueuedEvents());
    REQUIRE(fixture.scene.visibleFramingBounds().isNull());
    CHECK(colorBy.rangeMin() == Approx(kShotLowZ));
    CHECK(colorBy.rangeMax() == Approx(kShotsOnlySnappedHigh));
}
