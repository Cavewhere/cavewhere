//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwGltfLoader.h"
#include "cwGltfBaseColorTexture.h"
#include "cwCropImageTask.h"
#include "cwImage.h"
#include "cwDiskCacher.h"
#include "cwKtx2Codec.h"
#include "cwRenderTexturedItems.h"
#include "LoadProjectHelper.h"

//Qt includes
#include <QBuffer>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>

//AsyncFuture includes
#include <asyncfuture.h>

//Std includes
#include <cstring>

namespace {
constexpr int kExpectedVertexCount = 130010;
constexpr int kExpectedIndexCount = 130458;

int expectedStride(const QVector<cwGeometry::AttributeDesc>& layout)
{
    int stride = 0;
    for (const auto& desc : layout) {
        cwGeometry::VertexAttribute attr;
        attr.semantic = desc.semantic;
        attr.format = desc.format;
        stride += attr.byteSize();
    }
    return stride;
}

void requireLayoutMatches(const cwGeometry& geometry,
                          const QVector<cwGeometry::AttributeDesc>& layout)
{
    const auto& attributes = geometry.attributes();
    REQUIRE(attributes.size() == layout.size());

    int offset = 0;
    for (int i = 0; i < layout.size(); ++i) {
        const auto& expected = layout[i];
        const auto& actual = attributes[i];
        REQUIRE(actual.semantic == expected.semantic);
        REQUIRE(actual.format == expected.format);
        REQUIRE(actual.byteOffsetInBuffer == offset);
        offset += actual.byteSize();
    }

    const auto buffers = geometry.vertexBuffers();
    REQUIRE(buffers.size() == 1);
    REQUIRE(buffers[0].stride == offset);
    REQUIRE(buffers[0].stride == expectedStride(layout));
}
}

TEST_CASE("GLTF loader repacks geometry for textured items", "[cwGltfLoader]")
{
    const QString gltfPath = copyToTempFolder(testcasesDatasetPath("test_cwGltfLoader/test.glb"));
    REQUIRE_FALSE(gltfPath.isEmpty());
    REQUIRE(QFileInfo::exists(gltfPath));

    cw::gltf::LoadOptions options;
    options.requestedLayout = cwRenderTexturedItems::geometryLayout();

    const auto scene = cw::gltf::Loader::loadGltf(gltfPath, options);
    REQUIRE_FALSE(scene.meshes.isEmpty());

    bool sawGeometry = false;
    for (const auto& mesh : scene.meshes) {
        for (const auto& geometry : mesh.geometries) {
            sawGeometry = true;
            const auto buffers = geometry.vertexBuffers();
            REQUIRE(buffers.size() == 1);
            REQUIRE_FALSE(buffers[0].data->isEmpty());
            REQUIRE(geometry.vertexCount() == kExpectedVertexCount);
            REQUIRE(buffers[0].stride > 0);
            REQUIRE(geometry.indices().size() == kExpectedIndexCount);

            requireLayoutMatches(geometry, options.requestedLayout);

            REQUIRE(geometry.attribute(cwGeometry::Semantic::Position) != nullptr);
            REQUIRE(geometry.attribute(cwGeometry::Semantic::TexCoord0) != nullptr);
            REQUIRE(buffers[0].data->size() == geometry.vertexCount() * buffers[0].stride);
        }
    }

    REQUIRE(sawGeometry);
}

TEST_CASE("GLTF loader preserves full geometry when no options are provided", "[cwGltfLoader]")
{
    const QString gltfPath = copyToTempFolder(testcasesDatasetPath("test_cwGltfLoader/test.glb"));
    REQUIRE_FALSE(gltfPath.isEmpty());
    REQUIRE(QFileInfo::exists(gltfPath));

    const auto scene = cw::gltf::Loader::loadGltf(gltfPath);
    REQUIRE_FALSE(scene.meshes.isEmpty());

    bool sawGeometry = false;
    for (const auto& mesh : scene.meshes) {
        for (const auto& geometry : mesh.geometries) {
            sawGeometry = true;
            const auto buffers = geometry.vertexBuffers();
            REQUIRE(buffers.size() == 1);
            REQUIRE_FALSE(buffers[0].data->isEmpty());
            REQUIRE(geometry.vertexCount() == kExpectedVertexCount);
            REQUIRE(buffers[0].stride > 0);
            REQUIRE(geometry.indices().size() == kExpectedIndexCount);

            const auto* position = geometry.attribute(cwGeometry::Semantic::Position);
            const auto* texCoord0 = geometry.attribute(cwGeometry::Semantic::TexCoord0);
            const auto* normal = geometry.attribute(cwGeometry::Semantic::Normal);

            REQUIRE(position != nullptr);
            REQUIRE(texCoord0 != nullptr);
            REQUIRE(normal != nullptr);

            const auto& attributes = geometry.attributes();
            REQUIRE(attributes.size() == 3);
            REQUIRE(attributes[0].semantic == cwGeometry::Semantic::Position);
            REQUIRE(attributes[1].semantic == cwGeometry::Semantic::Normal);
            REQUIRE(attributes[2].semantic == cwGeometry::Semantic::TexCoord0);

            REQUIRE(position->format == cwGeometry::AttributeFormat::Vec3);
            REQUIRE(normal->format == cwGeometry::AttributeFormat::Vec3);
            REQUIRE(texCoord0->format == cwGeometry::AttributeFormat::Vec2);

            REQUIRE(buffers[0].stride == 32);
            REQUIRE(buffers[0].data->size() == geometry.vertexCount() * buffers[0].stride);
        }
    }

    REQUIRE(sawGeometry);
}

namespace {

//The encoded bytes a TextureCPU carries, in the same PNG form the loader keeps.
QByteArray encodedPng(const QImage& image)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if(!buffer.open(QIODevice::WriteOnly)) {
        return {};
    }
    image.save(&buffer, "PNG");
    return bytes;
}

cw::gltf::TextureCPU encodedTexture(const QImage& image)
{
    cw::gltf::TextureCPU texture;
    texture.width = image.width();
    texture.height = image.height();
    texture.isSRGB = true;
    texture.encodedPixels = encodedPng(image);
    return texture;
}

}

TEST_CASE("baseColorImage handles materials without a baseColor texture", "[cwGltfLoader]")
{
    constexpr int kTextureWidth = 4;
    constexpr int kTextureHeight = 2;

    QImage source(kTextureWidth, kTextureHeight, QImage::Format_RGBA8888);
    source.fill(Qt::white);

    cw::gltf::SceneCPU scene;
    scene.textures.append(encodedTexture(source));

    SECTION("valid index returns the texture's image") {
        cw::gltf::MaterialCPU material;
        material.baseColorTextureIndex = 0;

        const QImage image = cw::gltf::baseColorImage(scene, material);
        REQUIRE_FALSE(image.isNull());
        REQUIRE(image.width() == kTextureWidth);
        REQUIRE(image.height() == kTextureHeight);
    }

    SECTION("default material returns a null image") {
        const cw::gltf::MaterialCPU material;
        REQUIRE(material.baseColorTextureIndex == -1);
        REQUIRE(cw::gltf::baseColorImage(scene, material).isNull());
    }

    SECTION("out of range index returns a null image") {
        cw::gltf::MaterialCPU material;
        material.baseColorTextureIndex = scene.textures.size();
        REQUIRE(cw::gltf::baseColorImage(scene, material).isNull());
    }
}

TEST_CASE("GLTF loader keeps textures encoded until someone wants pixels", "[cwGltfLoader]")
{
    const QString gltfPath = copyToTempFolder(testcasesDatasetPath("test_cwGltfLoader/test.glb"));
    REQUIRE_FALSE(gltfPath.isEmpty());
    REQUIRE(QFileInfo::exists(gltfPath));

    cw::gltf::LoadOptions options;
    options.requestedLayout = cwRenderTexturedItems::geometryLayout();

    const auto scene = cw::gltf::Loader::loadGltf(gltfPath, options);
    REQUIRE_FALSE(scene.textures.isEmpty());
    const auto& texture = scene.textures.at(0);
    REQUIRE_FALSE(texture.encodedPixels.isEmpty());

    SECTION("the loader stores the encoded bytes, not a decoded copy") {
        //The header carries the dimensions, so the size is known without a decode
        REQUIRE(texture.width > 0);
        REQUIRE(texture.height > 0);

        const QImage reference = QImage::fromData(texture.encodedPixels);
        REQUIRE_FALSE(reference.isNull());
        CHECK(reference.size() == QSize(texture.width, texture.height));
    }

    SECTION("toImage decodes the same pixels the encoded bytes hold") {
        const QImage image = texture.toImage();
        REQUIRE_FALSE(image.isNull());
        CHECK(image.width() == texture.width);
        CHECK(image.height() == texture.height);
        CHECK(image.format() == QImage::Format_RGBA8888);

        const QImage reference =
            QImage::fromData(texture.encodedPixels).convertToFormat(QImage::Format_RGBA8888);
        REQUIRE_FALSE(reference.isNull());
        CHECK(std::memcmp(image.constBits(), reference.constBits(), reference.sizeInBytes()) == 0);
    }

    SECTION("each call decodes afresh instead of retaining pixels") {
        const QImage first = texture.toImage();
        const QImage second = texture.toImage();
        REQUIRE_FALSE(first.isNull());
        CHECK(first.constBits() != second.constBits());
    }
}

namespace {

constexpr int kCompressedTextureSize = 32;
constexpr int kSecondCompressedTextureSize = 16;
constexpr int kColorChannelMax = 255;

QImage compressibleImage(int size = kCompressedTextureSize)
{
    QImage image(size, size, QImage::Format_RGBA8888);
    for(int y = 0; y < image.height(); y++) {
        for(int x = 0; x < image.width(); x++) {
            const int red = x * kColorChannelMax / image.width();
            const int green = y * kColorChannelMax / image.height();
            image.setPixelColor(x, y, QColor(red, green, kColorChannelMax));
        }
    }
    return image;
}

//A stand-in for a .glb: cwGltfBaseColorTexture only reads the file's bytes to
//key the cache, so any bytes on disk make a valid source here.
QString writeGltfFile(const QDir& dataRootDir, const QByteArray& bytes)
{
    const QString path = dataRootDir.filePath(QStringLiteral("scan.glb"));
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    file.write(bytes);
    return path;
}

void appendBaseColor(cw::gltf::SceneCPU& scene, const QImage& image)
{
    scene.textures.append(encodedTexture(image));
}

cw::gltf::SceneCPU sceneWithBaseColor(const QImage& image)
{
    cw::gltf::SceneCPU scene;
    appendBaseColor(scene, image);
    return scene;
}

//Leaves a damaged cache entry behind, so a rewrite of the file is easy to spot.
bool truncateInHalf(const QString& path)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray bytes = file.readAll();
    file.close();

    if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(bytes.left(bytes.size() / 2));
    return true;
}

cwDiskCacher::Key textureKey(const QString& gltfPath, int textureIndex)
{
    const QFileInfo gltfInfo(gltfPath);
    return cwDiskCacher::Key {
        cw::ktx2::cacheKeyId(gltfInfo.fileName()
                             + QStringLiteral("-texture")
                             + QString::number(textureIndex))
            + QStringLiteral(".ktx2"),
        gltfInfo.dir(),
        QString()
    };
}

//What the render side would find behind a descriptor: the entry's level 0
QSize streamedLevelZeroSize(const cwStreamedTexture& streamed)
{
    const auto levels = cw::ktx2::loadStreamedLevels(streamed, QRhiTexture::RGBA8, 0);
    if(levels.hasError()) {
        return {};
    }
    return levels.value().size;
}

}

TEST_CASE("glTF base color textures reach the renderer as streamed descriptors",
          "[Gltf][cwGltfLoader]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QDir dataRootDir(tempDir.path());
    const QString gltfPath = writeGltfFile(dataRootDir, QByteArray("glb-bytes"));
    REQUIRE_FALSE(gltfPath.isEmpty());

    const cw::gltf::SceneCPU scene = sceneWithBaseColor(compressibleImage());
    cw::gltf::MaterialCPU material;
    material.baseColorTextureIndex = 0;

    SECTION("a cold cache encodes once and hands back the descriptor") {
        const cwGltfBaseColorTexture baseColorTexture(dataRootDir.path(), gltfPath);

        cwRenderTexturedItems::Item first;
        baseColorTexture.setOn(first, scene, material);
        REQUIRE_FALSE(first.streamedTexture.isNull());
        CHECK(first.streamedTexture.dataRootPath == dataRootDir.path());
        CHECK(first.streamedTexture.size == QSize(kCompressedTextureSize, kCompressedTextureSize));
        CHECK(first.texture.isNull());

        const cwDiskCacher cacher(dataRootDir);
        const cwDiskCacher::Key key = textureKey(gltfPath, 0);
        const QFileInfo cacheFile(cacher.filePath(key));
        REQUIRE(cacheFile.exists());
        CHECK(cacheFile.absoluteFilePath().contains(QStringLiteral("/.cw_cache/")));

        //The descriptor's size is the encoded header's, and it must agree with
        //the level 0 the render side will load
        CHECK(streamedLevelZeroSize(first.streamedTexture) == first.streamedTexture.size);
    }

    SECTION("a warm cache re-encodes nothing and repeats the descriptor") {
        const cwGltfBaseColorTexture cold(dataRootDir.path(), gltfPath);
        cwRenderTexturedItems::Item first;
        cold.setOn(first, scene, material);
        REQUIRE_FALSE(first.streamedTexture.isNull());

        const cwDiskCacher cacher(dataRootDir);
        const QString cachePath = cacher.filePath(textureKey(gltfPath, 0));
        const QFileInfo cacheFile(cachePath);
        REQUIRE(cacheFile.exists());
        const QDateTime firstWrite = cacheFile.lastModified();
        const qint64 firstSize = cacheFile.size();

        //A fresh instance, so the memo can't be what makes the second run cheap
        const cwGltfBaseColorTexture warm(dataRootDir.path(), gltfPath);
        cwRenderTexturedItems::Item second;
        warm.setOn(second, scene, material);

        CHECK(second.streamedTexture == first.streamedTexture);
        CHECK(second.texture.isNull());
        CHECK(QFileInfo(cachePath).lastModified() == firstWrite);
        CHECK(QFileInfo(cachePath).size() == firstSize);
    }

    SECTION("an edited scan re-encodes instead of trusting the stale entry") {
        const cwGltfBaseColorTexture original(dataRootDir.path(), gltfPath);
        cwRenderTexturedItems::Item first;
        original.setOn(first, scene, material);
        REQUIRE_FALSE(first.streamedTexture.isNull());

        REQUIRE_FALSE(writeGltfFile(dataRootDir, QByteArray("different-glb-bytes")).isEmpty());

        const cwGltfBaseColorTexture edited(dataRootDir.path(), gltfPath);
        cwRenderTexturedItems::Item second;
        edited.setOn(second, scene, material);
        REQUIRE_FALSE(second.streamedTexture.isNull());
        CHECK(second.streamedTexture.key.checksum != first.streamedTexture.key.checksum);
        CHECK(streamedLevelZeroSize(second.streamedTexture) == second.streamedTexture.size);
    }

    SECTION("a project without a data root keeps the decoded image") {
        const cwGltfBaseColorTexture baseColorTexture(QString(), gltfPath);
        cwRenderTexturedItems::Item item;
        baseColorTexture.setOn(item, scene, material);

        CHECK(item.streamedTexture.isNull());
        CHECK_FALSE(item.texture.isNull());
    }

    SECTION("bytes no decoder can read fall back to the image and are tried once") {
        cw::gltf::SceneCPU corruptScene;
        cw::gltf::TextureCPU corrupt;
        corrupt.width = kCompressedTextureSize;
        corrupt.height = kCompressedTextureSize;
        corrupt.isSRGB = true;
        corrupt.encodedPixels = QByteArray("not an image, and never was");
        corruptScene.textures.append(corrupt);

        const cwGltfBaseColorTexture baseColorTexture(dataRootDir.path(), gltfPath);

        cwRenderTexturedItems::Item first;
        baseColorTexture.setOn(first, corruptScene, material);
        CHECK(first.streamedTexture.isNull());
        CHECK(first.texture.isNull());

        const cwDiskCacher cacher(dataRootDir);
        CHECK_FALSE(QFileInfo::exists(cacher.filePath(textureKey(gltfPath, 0))));

        //The memoized failure is what keeps the second geometry from retrying
        cwRenderTexturedItems::Item second;
        baseColorTexture.setOn(second, corruptScene, material);
        CHECK(second.streamedTexture.isNull());
        CHECK_FALSE(QFileInfo::exists(cacher.filePath(textureKey(gltfPath, 0))));
    }
}

TEST_CASE("glTF base color textures read the cache once per task run", "[Gltf][cwGltfLoader]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QDir dataRootDir(tempDir.path());
    const QString gltfPath = writeGltfFile(dataRootDir, QByteArray("glb-bytes"));
    REQUIRE_FALSE(gltfPath.isEmpty());

    cw::gltf::SceneCPU scene = sceneWithBaseColor(compressibleImage());
    appendBaseColor(scene, compressibleImage(kSecondCompressedTextureSize));

    cw::gltf::MaterialCPU firstMaterial;
    firstMaterial.baseColorTextureIndex = 0;

    cw::gltf::MaterialCPU secondMaterial;
    secondMaterial.baseColorTextureIndex = 1;

    const cwDiskCacher cacher(dataRootDir);
    const cwGltfBaseColorTexture baseColorTexture(dataRootDir.path(), gltfPath);

    cwRenderTexturedItems::Item first;
    baseColorTexture.setOn(first, scene, firstMaterial);
    REQUIRE_FALSE(first.streamedTexture.isNull());

    SECTION("a second mesh on the same texture skips the cache file entirely") {
        const QString cachePath = cacher.filePath(textureKey(gltfPath, 0));
        REQUIRE(truncateInHalf(cachePath));
        const qint64 damagedSize = QFileInfo(cachePath).size();

        cwRenderTexturedItems::Item second;
        baseColorTexture.setOn(second, scene, firstMaterial);

        CHECK(second.streamedTexture == first.streamedTexture);
        CHECK(second.texture.isNull());

        //A re-read would have found the damaged entry and rewritten it.
        CHECK(QFileInfo(cachePath).size() == damagedSize);
    }

    SECTION("a mesh on another texture reads its own entry") {
        cwRenderTexturedItems::Item second;
        baseColorTexture.setOn(second, scene, secondMaterial);

        REQUIRE_FALSE(second.streamedTexture.isNull());
        CHECK(second.streamedTexture.size
              == QSize(kSecondCompressedTextureSize, kSecondCompressedTextureSize));
        CHECK(QFileInfo(cacher.filePath(textureKey(gltfPath, 1))).exists());
    }
}

TEST_CASE("Scrap and glTF cache keys share the encode generation", "[Gltf][cwGltfLoader][ScrapCompressedTexture]")
{
    constexpr int kCropTimeoutMilliseconds = 30000;
    constexpr int kNoteDotsPerMeter = 11811;

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QDir dataRootDir(tempDir.path());
    const QString gltfPath = writeGltfFile(dataRootDir, QByteArray("glb-bytes"));
    REQUIRE_FALSE(gltfPath.isEmpty());

    const cwGltfBaseColorTexture baseColorTexture(dataRootDir.path(), gltfPath);
    cw::gltf::MaterialCPU material;
    material.baseColorTextureIndex = 0;

    cwRenderTexturedItems::Item item;
    baseColorTexture.setOn(item, sceneWithBaseColor(compressibleImage()), material);
    REQUIRE_FALSE(item.streamedTexture.isNull());

    const QString notePath = dataRootDir.filePath(QStringLiteral("note.png"));
    REQUIRE(compressibleImage().save(notePath));

    cwImage note;
    note.setPath(notePath);
    note.setOriginalSize(QSize(kCompressedTextureSize, kCompressedTextureSize));
    note.setOriginalDotsPerMeter(kNoteDotsPerMeter);

    cwCropImageTask task;
    task.setDataRootDir(dataRootDir);
    task.setOriginal(note);
    task.setRectF(QRectF(0.0, 0.0, 1.0, 1.0));

    auto future = task.crop();
    REQUIRE(AsyncFuture::waitForFinished(future, kCropTimeoutMilliseconds));
    const cwCropImageTask::Result crop = future.result();
    REQUIRE_FALSE(crop.compressedKey.id.isEmpty());

    const QString suffix = cw::ktx2::cacheKeyId(QString());
    CHECK(item.streamedTexture.key.id.contains(suffix));
    CHECK(crop.compressedKey.id.contains(suffix));
}
