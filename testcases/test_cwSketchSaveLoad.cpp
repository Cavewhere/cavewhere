//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtLogging>

//Protobuf includes
#include <google/protobuf/util/json_util.h>

//Our includes
#include "cavewhere.pb.h"
#include "cwLength.h"
#include "cwPenPoint.h"
#include "cwPenStroke.h"
#include "cwProtoUtils.h"
#include "cwSaveLoad.h"
#include "cwScale.h"
#include "cwSketch.h"
#include "cwSketchData.h"
#include "cwSketchTestHelpers.h"
#include "cwUnits.h"

namespace {

void checkRoundTripEqual(const cwSketchData& in, const cwSketchData& out)
{
    CHECK(out.name     == in.name);
    CHECK(out.id       == in.id);
    CHECK(out.viewType == in.viewType);

    CHECK(out.mapScale.scaleNumerator.value   == in.mapScale.scaleNumerator.value);
    CHECK(out.mapScale.scaleNumerator.unit    == in.mapScale.scaleNumerator.unit);
    CHECK(out.mapScale.scaleDenominator.value == in.mapScale.scaleDenominator.value);
    CHECK(out.mapScale.scaleDenominator.unit  == in.mapScale.scaleDenominator.unit);

    REQUIRE(out.strokes.size() == in.strokes.size());
    for (int i = 0; i < in.strokes.size(); ++i) {
        const auto& a = in.strokes.at(i);
        const auto& b = out.strokes.at(i);
        CHECK(b.brushName == a.brushName);
        CHECK(b.id        == a.id);

        REQUIRE(b.points.size() == a.points.size());
        for (int j = 0; j < a.points.size(); ++j) {
            CHECK(b.points.at(j).position    == a.points.at(j).position);
            CHECK(b.points.at(j).pressure    == a.points.at(j).pressure);
            CHECK(b.points.at(j).timestampMs == a.points.at(j).timestampMs);
        }
    }
}

} // namespace

TEST_CASE("cwSketch proto in-memory round trip", "[cwSketchSaveLoad]") {
    cwSketchData in = makeSampleSketchData();

    cwSketch sketch;
    sketch.setData(in);

    auto protoSketch = cwSaveLoad::toProtoSketch(&sketch);
    REQUIRE(protoSketch != nullptr);

    cwSketchData out = cwSaveLoad::sketchDataFromProtoSketch(*protoSketch, QString());

    checkRoundTripEqual(in, out);
}

TEST_CASE("cwSketch file round trip via loadSketch", "[cwSketchSaveLoad]") {
    cwSketchData in = makeSampleSketchData();

    cwSketch sketch;
    sketch.setData(in);

    auto protoSketch = cwSaveLoad::toProtoSketch(&sketch);
    REQUIRE(protoSketch != nullptr);

    QTemporaryDir tempDir(QDir::tempPath()
                          + QStringLiteral("/test_cwSketchSaveLoad-%1-XXXXXX")
                            .arg(QCoreApplication::applicationPid()));
    REQUIRE(tempDir.isValid());

    const QString path = tempDir.filePath(QStringLiteral("sample.cwsketch"));

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

    checkRoundTripEqual(in, loaded.value());
}

TEST_CASE("cwSketch viewType fallback warns and drops to Plan", "[cwSketchSaveLoad]") {
    CavewhereProto::Sketch proto;
    cwProtoUtils::saveQUuid(proto.mutable_id(), QUuid::createUuid());
    proto.set_name("Running-profile sketch");
    proto.set_viewtype(CavewhereProto::Sketch_ViewType_RunningProfile);

    // The fallback warning is emitted via qWarning(); we assert the resulting
    // behaviour rather than capturing the warning string. Suppress the noise
    // from the test log by swallowing warnings for the duration of the call.
    auto priorHandler = qInstallMessageHandler(
        [](QtMsgType type, const QMessageLogContext&, const QString&) {
            Q_UNUSED(type)
        });

    cwSketchData data = cwProtoUtils::fromProtoSketch(proto);

    qInstallMessageHandler(priorHandler);

    CHECK(data.viewType == cwSketchData::Plan);
    CHECK(data.name == QStringLiteral("Running-profile sketch"));

    // ProjectedProfile path
    proto.set_viewtype(CavewhereProto::Sketch_ViewType_ProjectedProfile);
    priorHandler = qInstallMessageHandler(
        [](QtMsgType, const QMessageLogContext&, const QString&) {});
    data = cwProtoUtils::fromProtoSketch(proto);
    qInstallMessageHandler(priorHandler);
    CHECK(data.viewType == cwSketchData::Plan);
}
