/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwRenderLinePlot.h"

//Qt includes
#include <QFile>
#include <QRegularExpression>
#include <rhi/qshader.h>

//Std includes
#include <cmath>

namespace {

constexpr int kSegmentCount = 6;
// QColor stores 16 bits per channel; the shader prints floats in full.
constexpr double kColorTolerance = 1e-3;
const QString kLinePlotFragment = QStringLiteral(":/shaders/LinePlot.frag.qsb");
const QShaderVersion kGlslVersion(150);

QVector<QVector3D> segmentPoints(int segmentCount)
{
    QVector<QVector3D> points;
    for (int segment = 0; segment < segmentCount; ++segment) {
        points << QVector3D(segment, 0.0f, 0.0f) << QVector3D(segment, 1.0f, 0.0f);
    }
    return points;
}

// The vec4 entries of the GLSL constant array the fragment shader compiled
// kTypeColors into.
QVector<QColor> shaderPalette()
{
    QFile file(kLinePlotFragment);
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QShader shader = QShader::fromSerialized(file.readAll());
    REQUIRE(shader.isValid());
    const QString source = QString::fromUtf8(shader.shader(QShaderKey(QShader::GlslShader, kGlslVersion)).shader());
    INFO(source.toStdString());

    const QString arrayStart = QStringLiteral("[%1] = vec4[](").arg(cwRenderLinePlot::kSegmentTypeCount);
    const qsizetype start = source.indexOf(arrayStart);
    REQUIRE(start >= 0);
    const qsizetype end = source.indexOf(QStringLiteral(");"), start);
    REQUIRE(end > start);
    const QString array = source.mid(start + arrayStart.size(), end - start - arrayStart.size());

    QVector<QColor> palette;
    static const QRegularExpression vec4(QStringLiteral(R"(vec4\(([^()]*)\))"));
    for (QRegularExpressionMatchIterator it = vec4.globalMatch(array); it.hasNext();) {
        const QStringList channels = it.next().captured(1).split(QLatin1Char(','));
        REQUIRE(channels.size() == 4);
        palette.append(QColor::fromRgbF(channels.at(0).toFloat(),
                                        channels.at(1).toFloat(),
                                        channels.at(2).toFloat(),
                                        channels.at(3).toFloat()));
    }
    return palette;
}

bool isNear(const QColor& a, const QColor& b)
{
    return std::abs(a.redF() - b.redF()) < kColorTolerance
           && std::abs(a.greenF() - b.greenF()) < kColorTolerance
           && std::abs(a.blueF() - b.blueF()) < kColorTolerance
           && std::abs(a.alphaF() - b.alphaF()) < kColorTolerance;
}

} // namespace

TEST_CASE("cwRenderLinePlot types each segment by its range", "[PassageTube][cwRenderLinePlot]")
{
    using Range = cwLinePlotGeometry::VertexRange;
    cwRenderLinePlot plot;

    SECTION("typed ranges")
    {
        plot.setGeometry(segmentPoints(kSegmentCount),
                         QVector<cwRenderLinePlot::TypedRange> {
                             {Range {0, 4}, cwRenderLinePlot::kRingSegment},
                             {Range {6, 2}, cwRenderLinePlot::kFrameSegment},
                             {Range {8, 4}, cwRenderLinePlot::kOvershootSegment},
                         });
        CHECK(plot.segmentTypes() == QVector<quint32> {
                  cwRenderLinePlot::kRingSegment,
                  cwRenderLinePlot::kRingSegment,
                  cwRenderLinePlot::kCenterlineSegment,
                  cwRenderLinePlot::kFrameSegment,
                  cwRenderLinePlot::kOvershootSegment,
                  cwRenderLinePlot::kOvershootSegment,
              });
    }

    SECTION("splay ranges")
    {
        plot.setGeometry(segmentPoints(kSegmentCount), QVector<Range> {{2, 4}});
        CHECK(plot.segmentTypes() == QVector<quint32> {
                  cwRenderLinePlot::kCenterlineSegment,
                  cwRenderLinePlot::kSplaySegment,
                  cwRenderLinePlot::kSplaySegment,
                  cwRenderLinePlot::kCenterlineSegment,
                  cwRenderLinePlot::kCenterlineSegment,
                  cwRenderLinePlot::kCenterlineSegment,
              });
    }
}

TEST_CASE("cwRenderLinePlot's palette matches LinePlot.frag", "[PassageTube][cwRenderLinePlot]")
{
    const QVector<QColor> palette = shaderPalette();
    REQUIRE(palette.size() == static_cast<qsizetype>(cwRenderLinePlot::kSegmentTypeCount));
    for (quint32 type = 0; type < cwRenderLinePlot::kSegmentTypeCount; ++type) {
        INFO("type " << type << " shader " << palette.at(type).name(QColor::HexArgb).toStdString()
                     << " table " << cwRenderLinePlot::segmentColor(type).name(QColor::HexArgb).toStdString());
        CHECK(isNear(palette.at(type), cwRenderLinePlot::segmentColor(type)));
    }

    SECTION("every type has its own color")
    {
        for (quint32 a = 0; a < cwRenderLinePlot::kSegmentTypeCount; ++a) {
            for (quint32 b = a + 1; b < cwRenderLinePlot::kSegmentTypeCount; ++b) {
                CHECK(cwRenderLinePlot::segmentColor(a) != cwRenderLinePlot::segmentColor(b));
            }
        }
    }
}
