#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUuid>

#include <google/protobuf/util/json_util.h>

#include "cavewhere.pb.h"
#include "cwLazLayer.h"
#include "cwLazLayerData.h"
#include "cwLazLayerModel.h"
#include "cwSaveLoad.h"

namespace {

QString writeCwlazJson(const QDir& dir, const QString& filename,
                      const CavewhereProto::LazLayer& proto)
{
    const QString path = dir.absoluteFilePath(filename);
    std::string json;
    google::protobuf::util::JsonPrintOptions opts;
    opts.add_whitespace = true;
    REQUIRE(google::protobuf::util::MessageToJsonString(proto, &json, opts).ok());

    QFile file(path);
    const bool opened = file.open(QFile::WriteOnly | QFile::Truncate);
    REQUIRE(opened);
    file.write(json.data(), qint64(json.size()));
    file.close();
    return path;
}

} // namespace

TEST_CASE("cwSaveLoad::toProtoLazLayer captures id and enabled",
          "[cwSaveLoad][cwLazLayerIO]") {
    cwLazLayer layer;
    layer.setEnabled(false);

    auto proto = cwSaveLoad::toProtoLazLayer(&layer);
    REQUIRE(proto != nullptr);

    REQUIRE(proto->has_enabled());
    REQUIRE(proto->enabled() == false);

    REQUIRE(proto->has_id());
    const QString protoId = QString::fromStdString(proto->id());
    REQUIRE(QUuid::fromString(protoId) == layer.id());
}

TEST_CASE("cwSaveLoad::lazLayerDataFromProtoLazLayer reads id and enabled, defaults absent fields",
          "[cwSaveLoad][cwLazLayerIO]") {
    SECTION("All fields present") {
        CavewhereProto::LazLayer proto;
        const QUuid uuid = QUuid::createUuid();
        proto.set_id(uuid.toString(QUuid::WithoutBraces).toStdString());
        proto.set_enabled(false);

        const cwLazLayerData data = cwSaveLoad::lazLayerDataFromProtoLazLayer(
                    proto, QStringLiteral("foo.cwlaz"));

        REQUIRE(data.id == uuid);
        REQUIRE(data.enabled == false);
        REQUIRE(data.fileName == QStringLiteral("foo"));
    }

    SECTION("Missing enabled field defaults to true") {
        CavewhereProto::LazLayer proto;
        const QUuid uuid = QUuid::createUuid();
        proto.set_id(uuid.toString(QUuid::WithoutBraces).toStdString());

        const cwLazLayerData data = cwSaveLoad::lazLayerDataFromProtoLazLayer(
                    proto, QStringLiteral("foo.cwlaz"));

        REQUIRE(data.id == uuid);
        REQUIRE(data.enabled == true);
    }

    SECTION("Missing id field yields null UUID") {
        CavewhereProto::LazLayer proto;
        proto.set_enabled(false);

        const cwLazLayerData data = cwSaveLoad::lazLayerDataFromProtoLazLayer(
                    proto, QStringLiteral("foo.cwlaz"));

        REQUIRE(data.id.isNull());
        REQUIRE(data.enabled == false);
    }

    SECTION("Filename pairing uses completeBaseName for compound names") {
        CavewhereProto::LazLayer proto;
        const cwLazLayerData data = cwSaveLoad::lazLayerDataFromProtoLazLayer(
                    proto, QStringLiteral("alpha.beta.cwlaz"));
        REQUIRE(data.fileName == QStringLiteral("alpha.beta"));
    }
}

TEST_CASE("cwSaveLoad::loadLazLayer round-trip preserves UUID byte-for-byte",
          "[cwSaveLoad][cwLazLayerIO]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QUuid uuid = QUuid::createUuid();
    CavewhereProto::LazLayer proto;
    proto.set_id(uuid.toString(QUuid::WithoutBraces).toStdString());
    proto.set_enabled(false);

    const QString path = writeCwlazJson(QDir(tempDir.path()),
                                        QStringLiteral("rt-%1.cwlaz")
                                                .arg(QCoreApplication::applicationPid()),
                                        proto);

    auto result = cwSaveLoad::loadLazLayer(path);
    REQUIRE_FALSE(result.hasError());

    const cwLazLayerData data = result.value();
    REQUIRE(data.id == uuid);
    REQUIRE(data.enabled == false);
}

TEST_CASE("cwSaveLoad::loadLazLayer returns error for a path that doesn't exist",
          "[cwSaveLoad][cwLazLayerIO]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString missingPath = QDir(tempDir.path()).absoluteFilePath(
                QStringLiteral("does-not-exist-%1.cwlaz")
                        .arg(QCoreApplication::applicationPid()));
    REQUIRE_FALSE(QFileInfo::exists(missingPath));

    auto result = cwSaveLoad::loadLazLayer(missingPath);
    REQUIRE(result.hasError());
}

TEST_CASE("cwSaveLoad::loadLazLayer treats an empty file as default state",
          "[cwSaveLoad][cwLazLayerIO]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = QDir(tempDir.path()).absoluteFilePath(
                QStringLiteral("empty-%1.cwlaz")
                        .arg(QCoreApplication::applicationPid()));
    {
        QFile file(path);
        const bool opened = file.open(QFile::WriteOnly | QFile::Truncate);
        REQUIRE(opened);
        file.close();
    }
    REQUIRE(QFileInfo(path).size() == 0);

    auto result = cwSaveLoad::loadLazLayer(path);
    REQUIRE_FALSE(result.hasError());

    const cwLazLayerData data = result.value();
    REQUIRE(data.id.isNull());
    REQUIRE(data.enabled == true);
    REQUIRE(data.fileName == QStringLiteral("empty-%1")
                .arg(QCoreApplication::applicationPid()));
}

TEST_CASE("cwSaveLoad::loadLazLayer rejects a present-but-malformed id",
          "[cwSaveLoad][cwLazLayerIO]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = QDir(tempDir.path()).absoluteFilePath(
                QStringLiteral("badid-%1.cwlaz")
                        .arg(QCoreApplication::applicationPid()));
    {
        QFile file(path);
        const bool opened = file.open(QFile::WriteOnly | QFile::Truncate);
        REQUIRE(opened);
        file.write("{\"id\": \"not-a-uuid\", \"enabled\": true}");
        file.close();
    }

    auto result = cwSaveLoad::loadLazLayer(path);
    REQUIRE(result.hasError());
    REQUIRE(result.errorMessage().contains(QStringLiteral("Malformed id")));
}

TEST_CASE("cwSaveLoad::loadLazLayer returns error for malformed JSON",
          "[cwSaveLoad][cwLazLayerIO]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = QDir(tempDir.path()).absoluteFilePath(
                QStringLiteral("garbage-%1.cwlaz")
                        .arg(QCoreApplication::applicationPid()));
    {
        QFile file(path);
        const bool opened = file.open(QFile::WriteOnly | QFile::Truncate);
        REQUIRE(opened);
        file.write("{this is not valid json");
        file.close();
    }

    auto result = cwSaveLoad::loadLazLayer(path);
    REQUIRE(result.hasError());
}

TEST_CASE("cwSaveLoad::save(cwLazLayer*) writes a .cwlaz that loadLazLayer reads back",
          "[cwSaveLoad][cwLazLayerIO]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    // Build a minimal project layout: <root>/proj.cwproj/GIS Layers/.
    const QString projectDir = QDir(tempDir.path())
            .absoluteFilePath(QStringLiteral("proj-%1.cwproj")
                              .arg(QCoreApplication::applicationPid()));
    REQUIRE(QDir().mkpath(projectDir));
    const QString gisLayersDir = QDir(projectDir)
            .absoluteFilePath(cwLazLayerModel::folderName());
    REQUIRE(QDir().mkpath(gisLayersDir));
    const QString projectFile = QDir(projectDir)
            .absoluteFilePath(QStringLiteral("proj.cwproj"));

    cwSaveLoad saveLoad;
    saveLoad.setFileName(projectFile);

    // The .laz source doesn't need real bytes for this test — the save path
    // only writes the .cwlaz metadata. Disable the layer first so reload()
    // is a no-op (and we get a known-non-default value to round-trip).
    cwLazLayer layer;
    layer.setEnabled(false);
    const QString lazPath = QDir(gisLayersDir).absoluteFilePath(QStringLiteral("alpha.laz"));
    layer.setSourcePath(lazPath);

    const QUuid originalId = layer.id();
    REQUIRE_FALSE(originalId.isNull());

    saveLoad.save(&layer);
    saveLoad.waitForFinished();

    const QString cwlazPath = QDir(gisLayersDir).absoluteFilePath(QStringLiteral("alpha.cwlaz"));
    REQUIRE(QFileInfo::exists(cwlazPath));

    auto loaded = cwSaveLoad::loadLazLayer(cwlazPath);
    REQUIRE_FALSE(loaded.hasError());
    REQUIRE(loaded.value().id == originalId);
    REQUIRE(loaded.value().enabled == false);
    REQUIRE(loaded.value().fileName == QStringLiteral("alpha"));
}

TEST_CASE("cwSaveLoad::save(cwLazLayer*) sanitizes filenames with spaces and parentheses",
          "[cwSaveLoad][cwLazLayerIO]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString projectDir = QDir(tempDir.path())
            .absoluteFilePath(QStringLiteral("proj-sanitize-%1.cwproj")
                              .arg(QCoreApplication::applicationPid()));
    REQUIRE(QDir().mkpath(projectDir));
    const QString gisLayersDir = QDir(projectDir)
            .absoluteFilePath(cwLazLayerModel::folderName());
    REQUIRE(QDir().mkpath(gisLayersDir));
    const QString projectFile = QDir(projectDir)
            .absoluteFilePath(QStringLiteral("proj.cwproj"));

    cwSaveLoad saveLoad;
    saveLoad.setFileName(projectFile);

    cwLazLayer layer;
    layer.setEnabled(true);
    // Picks a name with characters that filesystems on Windows reject and
    // sanitizeFileName has to scrub. The pairing rule on Mac/Linux preserves
    // spaces and parens, so the resulting basename of the .cwlaz must match
    // what cwLazLayerModel::rescan would compute for the same .laz.
    const QString lazPath = QDir(gisLayersDir).absoluteFilePath(
                QStringLiteral("clip (002) raw.laz"));
    layer.setSourcePath(lazPath);

    saveLoad.save(&layer);
    saveLoad.waitForFinished();

    // The on-disk basename is whatever sanitizeFileName decides. We don't
    // care about the exact sanitization rules — what we care about is that
    // there is exactly one .cwlaz in GIS Layers/ after the save, and it
    // round-trips through loadLazLayer cleanly.
    const QStringList entries = QDir(gisLayersDir).entryList(
                {QStringLiteral("*.cwlaz")}, QDir::Files);
    REQUIRE(entries.size() == 1);

    const QString cwlazPath = QDir(gisLayersDir).absoluteFilePath(entries.first());
    auto loaded = cwSaveLoad::loadLazLayer(cwlazPath);
    REQUIRE_FALSE(loaded.hasError());
    REQUIRE(loaded.value().id == layer.id());
}

TEST_CASE("cwLazLayerData round-trips id through cwLazLayer::data/setData",
          "[cwLazLayer][cwLazLayerIO]") {
    SECTION("Non-null id overrides the auto-generated UUID") {
        cwLazLayer layer;
        const QUuid original = layer.id();
        const QUuid newId = QUuid::createUuid();
        REQUIRE(original != newId);

        cwLazLayerData data;
        data.id = newId;
        data.enabled = false;
        layer.setData(data);

        REQUIRE(layer.id() == newId);
        REQUIRE(layer.enabled() == false);
    }

    SECTION("Null id leaves the auto-generated UUID untouched") {
        cwLazLayer layer;
        const QUuid original = layer.id();
        REQUIRE_FALSE(original.isNull());

        cwLazLayerData data; // id is null by default
        data.enabled = false;
        layer.setData(data);

        REQUIRE(layer.id() == original);
        REQUIRE(layer.enabled() == false);
    }

    SECTION("data() reports the layer's current id") {
        cwLazLayer layer;
        const cwLazLayerData snapshot = layer.data();
        REQUIRE(snapshot.id == layer.id());
        REQUIRE(snapshot.enabled == true);
    }
}
