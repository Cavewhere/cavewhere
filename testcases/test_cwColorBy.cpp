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

using namespace Catch;

namespace {

constexpr float kDeepest = -500.0f;
constexpr float kHighest = 100.0f;
constexpr float kFlatZ = -42.0f;
constexpr double kMovedPivot = -200.0;
constexpr double kMovedHalfExtent = 300.0;
constexpr double kStoredPivot = -75.5;
constexpr double kStoredScrapStrength = 0.25;

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
