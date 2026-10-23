//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwColorRamp.h"

//Qt includes
#include <QColor>
#include <QSet>

//Std includes
#include <cstdlib>

namespace {

constexpr int kChannelTolerance = 1; // one step of 8-bit color
constexpr int kBakeCount = 32;
constexpr int kOkabeItoSlotCount = 8;
constexpr double kLightnessStep = 0.1;
constexpr int kLightnessSamples = 11;

bool closeColor(const QColor& a, const QColor& b)
{
    return std::abs(a.red() - b.red()) <= kChannelTolerance
           && std::abs(a.green() - b.green()) <= kChannelTolerance
           && std::abs(a.blue() - b.blue()) <= kChannelTolerance;
}

}

TEST_CASE("cwColorRamp OKLab round trips sRGB colors", "[ColorBy]")
{
    const QStringList colors = {
        QStringLiteral("#000000"),
        QStringLiteral("#ffffff"),
        QStringLiteral("#ff0000"),
        QStringLiteral("#2563eb"),
        QStringLiteral("#d9a066")
    };

    for (const QString& name : colors) {
        INFO("Color " << name.toStdString());
        const QColor color(name);
        const QColor roundTrip = cwColorRamp::fromOklab(cwColorRamp::toOklab(color));
        CHECK(closeColor(roundTrip, color));
    }
}

TEST_CASE("cwColorRamp colorAt hits the end stops and the white pivot", "[ColorBy]")
{
    const cwColorRamp depth = cwColorRamp::preset(cwColorRamp::defaultDepthRampId());
    REQUIRE(depth.isValid());
    CHECK(depth.kind() == cwColorRamp::Kind::Diverging);

    CHECK(closeColor(depth.colorAt(0.0), depth.stops().first().color));
    CHECK(closeColor(depth.colorAt(1.0), depth.stops().last().color));
    CHECK(closeColor(depth.colorAt(0.5), QColor(Qt::white)));

    // Out-of-range t clamps to the ends
    CHECK(closeColor(depth.colorAt(-1.0), depth.stops().first().color));
    CHECK(closeColor(depth.colorAt(2.0), depth.stops().last().color));
}

TEST_CASE("cwColorRamp blends a two-stop ramp with monotonic lightness", "[ColorBy]")
{
    const cwColorRamp blackToWhite(QStringLiteral("bw"), QStringLiteral("Black to white"),
                                   cwColorRamp::Kind::Sequential,
                                   {{0.0, QColor(Qt::black)}, {1.0, QColor(Qt::white)}});
    REQUIRE(blackToWhite.isValid());

    double previous = -1.0;
    for (int i = 0; i < kLightnessSamples; ++i) {
        const double t = i * kLightnessStep;
        const double lightness = cwColorRamp::toOklab(blackToWhite.colorAt(t)).l;
        INFO("t = " << t);
        CHECK(lightness > previous);
        previous = lightness;
    }
}

TEST_CASE("cwColorRamp bake samples the ramp", "[ColorBy]")
{
    const cwColorRamp depth = cwColorRamp::preset(cwColorRamp::defaultDepthRampId());
    const QVector<QColor> depthSamples = depth.bake(kBakeCount);
    CHECK(depthSamples.size() == kBakeCount);
    CHECK(closeColor(depthSamples.first(), depth.stops().first().color));
    CHECK(closeColor(depthSamples.last(), depth.stops().last().color));

    const cwColorRamp okabeIto = cwColorRamp::preset(cwColorRamp::defaultCategoricalRampId());
    REQUIRE(okabeIto.isValid());
    CHECK(okabeIto.kind() == cwColorRamp::Kind::Categorical);
    CHECK(okabeIto.stops().size() == kOkabeItoSlotCount);

    const QVector<QColor> categoricalSamples = okabeIto.bake(kBakeCount);
    CHECK(categoricalSamples.size() == kBakeCount);
    CHECK(categoricalSamples.at(kOkabeItoSlotCount) == categoricalSamples.at(0));
    for (int i = 0; i < kOkabeItoSlotCount; ++i) {
        CHECK(categoricalSamples.at(i) == okabeIto.stops().at(i).color);
    }
}

TEST_CASE("cwColorRamp presets are valid and uniquely named", "[ColorBy]")
{
    CHECK_FALSE(cwColorRamp::preset(QStringLiteral("nope")).isValid());

    QSet<QString> ids;
    const QVector<cwColorRamp> presets = cwColorRamp::presets();
    for (const cwColorRamp& ramp : presets) {
        INFO("Ramp " << ramp.id().toStdString());
        CHECK(ramp.isValid());
        CHECK_FALSE(ramp.name().isEmpty());
        CHECK_FALSE(ids.contains(ramp.id()));
        ids.insert(ramp.id());
        CHECK(cwColorRamp::preset(ramp.id()).id() == ramp.id());
    }
}
