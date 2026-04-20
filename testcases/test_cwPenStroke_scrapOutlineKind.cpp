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

cwSketchData makeOutlineSketchData(const QUuid &strokeId)
{
    cwSketchData data;
    data.name     = QStringLiteral("outline-sketch");
    data.id       = QUuid::createUuid();
    data.viewType = cwSketchData::Plan;
    data.mapScale.scaleNumerator.unit    = cwUnits::Meters;
    data.mapScale.scaleNumerator.value   = 1.0;
    data.mapScale.scaleDenominator.unit  = cwUnits::Meters;
    data.mapScale.scaleDenominator.value = 200.0;

    cwPenStroke outline;
    outline.kind  = cwPenStroke::ScrapOutline;
    outline.width = 3.0;
    outline.color = QColor("#ff445566");
    outline.id    = strokeId;
    outline.points.append(cwPenPoint(QPointF(0.0, 0.0), 1.0, 100));
    outline.points.append(cwPenPoint(QPointF(1.0, 0.0), 1.0, 101));
    outline.points.append(cwPenPoint(QPointF(1.0, 1.0), 1.0, 102));
    outline.points.append(cwPenPoint(QPointF(0.0, 0.0), 1.0, 103));
    data.strokes.append(outline);
    return data;
}

} // namespace

TEST_CASE("cwPenStroke::Kind::ScrapOutline value is stable", "[cwPenStroke][ScrapOutline]") {
    CHECK(static_cast<int>(cwPenStroke::Wall)         == 0);
    CHECK(static_cast<int>(cwPenStroke::Feature)      == 1);
    CHECK(static_cast<int>(cwPenStroke::ScrapOutline) == 2);

    // Proto enum must match the C++ enum; the serializer casts directly
    // between them in cwProtoUtils.
    CHECK(static_cast<int>(CavewhereProto::PenStroke_Kind_Wall)         == 0);
    CHECK(static_cast<int>(CavewhereProto::PenStroke_Kind_Feature)      == 1);
    CHECK(static_cast<int>(CavewhereProto::PenStroke_Kind_ScrapOutline) == 2);
}

TEST_CASE("cwPenStroke ScrapOutline kind round-trips through proto", "[cwPenStroke][ScrapOutline]") {
    const QUuid strokeId = QUuid::createUuid();
    cwSketch sketch;
    sketch.setData(makeOutlineSketchData(strokeId));

    auto protoSketch = cwSaveLoad::toProtoSketch(&sketch);
    REQUIRE(protoSketch != nullptr);
    REQUIRE(protoSketch->strokes_size() == 1);
    CHECK(protoSketch->strokes(0).kind()
          == CavewhereProto::PenStroke_Kind_ScrapOutline);

    cwSketchData out = cwSaveLoad::sketchDataFromProtoSketch(*protoSketch, QString());
    REQUIRE(out.strokes.size() == 1);
    CHECK(out.strokes.first().kind  == cwPenStroke::ScrapOutline);
    CHECK(out.strokes.first().width == 3.0);
    CHECK(out.strokes.first().id    == strokeId);
}

TEST_CASE("cwPenStroke ScrapOutline survives .cwsketch file load", "[cwPenStroke][ScrapOutline]") {
    const QUuid strokeId = QUuid::createUuid();
    cwSketch sketch;
    sketch.setData(makeOutlineSketchData(strokeId));

    auto protoSketch = cwSaveLoad::toProtoSketch(&sketch);
    REQUIRE(protoSketch != nullptr);

    QTemporaryDir tempDir(QDir::tempPath()
                          + QStringLiteral("/test_cwPenStroke_scrapOutlineKind-%1-XXXXXX")
                            .arg(QCoreApplication::applicationPid()));
    REQUIRE(tempDir.isValid());
    const QString path = tempDir.filePath(QStringLiteral("outline.cwsketch"));

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
    CHECK(out.strokes.first().kind == cwPenStroke::ScrapOutline);
    CHECK(out.strokes.first().id   == strokeId);
}
