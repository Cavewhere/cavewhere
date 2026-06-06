//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

//Protobuf includes
#include <google/protobuf/util/json_util.h>

//Our includes
#include "cavewhere.pb.h"
#include "cwPenPoint.h"
#include "cwPenStroke.h"
#include "cwProtoUtils.h"
#include "cwSaveLoad.h"
#include "cwSketch.h"
#include "cwSketchData.h"
#include "cwUnits.h"

namespace {

cwSketchData makeBrushSketchData(const QUuid &strokeId, const QString &brushName)
{
    cwSketchData data;
    data.name     = QStringLiteral("brush-sketch");
    data.id       = QUuid::createUuid();
    data.viewType = cwSketchData::Plan;
    data.mapScale.scaleNumerator.unit    = cwUnits::Meters;
    data.mapScale.scaleNumerator.value   = 1.0;
    data.mapScale.scaleDenominator.unit  = cwUnits::Meters;
    data.mapScale.scaleDenominator.value = 200.0;

    cwPenStroke stroke;
    stroke.brushName = brushName;
    stroke.id        = strokeId;
    stroke.points.append(cwPenPoint(QPointF(0.0, 0.0), 1.0, 100));
    stroke.points.append(cwPenPoint(QPointF(1.0, 0.0), 1.0, 101));
    stroke.points.append(cwPenPoint(QPointF(1.0, 1.0), 1.0, 102));
    data.strokes.append(stroke);
    return data;
}

} // namespace

TEST_CASE("cwPenStroke brushName round-trips through proto", "[cwPenStroke][brushName]") {
    const QUuid strokeId = QUuid::createUuid();
    cwSketch sketch;
    sketch.setData(makeBrushSketchData(strokeId, QStringLiteral("scrap-outline")));

    auto protoSketch = cwSaveLoad::toProtoSketch(&sketch);
    REQUIRE(protoSketch != nullptr);
    REQUIRE(protoSketch->strokes_size() == 1);
    CHECK(protoSketch->strokes(0).brushname() == "scrap-outline");

    cwSketchData out = cwSaveLoad::sketchDataFromProtoSketch(*protoSketch, QString());
    REQUIRE(out.strokes.size() == 1);
    CHECK(out.strokes.first().brushName == QStringLiteral("scrap-outline"));
    CHECK(out.strokes.first().id        == strokeId);
}

TEST_CASE("cwPenStroke brushName survives .cwsketch file load", "[cwPenStroke][brushName]") {
    const QUuid strokeId = QUuid::createUuid();
    cwSketch sketch;
    sketch.setData(makeBrushSketchData(strokeId, QStringLiteral("wall")));

    auto protoSketch = cwSaveLoad::toProtoSketch(&sketch);
    REQUIRE(protoSketch != nullptr);

    QTemporaryDir tempDir(QDir::tempPath()
                          + QStringLiteral("/test_cwPenStroke_brushName-%1-XXXXXX")
                            .arg(QCoreApplication::applicationPid()));
    REQUIRE(tempDir.isValid());
    const QString path = tempDir.filePath(QStringLiteral("brush.cwsketch"));

    std::string json;
    google::protobuf::util::JsonPrintOptions jsonOptions;
    REQUIRE(google::protobuf::util::MessageToJsonString(*protoSketch, &json, jsonOptions).ok());

    {
        QFile f(path);
        REQUIRE(f.open(QIODevice::WriteOnly));
        f.write(json.data(), static_cast<qint64>(json.size()));
    }

    auto loaded = cwSaveLoad::loadSketch(path, QDir(tempDir.path()));
    REQUIRE(!loaded.hasError());

    const cwSketchData out = loaded.value();
    REQUIRE(out.strokes.size() == 1);
    CHECK(out.strokes.first().brushName == QStringLiteral("wall"));
    CHECK(out.strokes.first().id        == strokeId);
}
