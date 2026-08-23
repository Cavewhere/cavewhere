//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwGltfLoader.h"
#include "cwGltfBaseColorTexture.h"
#include "cwDiskCacher.h"
#include "cwKtx2Codec.h"
#include "cwRenderTexturedItems.h"
#include "LoadProjectHelper.h"

//Qt includes
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>

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

TEST_CASE("baseColorImage handles materials without a baseColor texture", "[cwGltfLoader]")
{
    constexpr int kTextureWidth = 4;
    constexpr int kTextureHeight = 2;

    cw::gltf::TextureCPU texture;
    texture.width = kTextureWidth;
    texture.height = kTextureHeight;
    texture.isSRGB = true;
    texture.pixels = QByteArray(kTextureWidth * kTextureHeight * 4, char(0xFF));

    cw::gltf::SceneCPU scene;
    scene.textures.append(texture);

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

TEST_CASE("GLTF loader shares texture pixels with the images it hands out", "[cwGltfLoader]")
{
    const QString gltfPath = copyToTempFolder(testcasesDatasetPath("test_cwGltfLoader/test.glb"));
    REQUIRE_FALSE(gltfPath.isEmpty());
    REQUIRE(QFileInfo::exists(gltfPath));

    cw::gltf::LoadOptions options;
    options.requestedLayout = cwRenderTexturedItems::geometryLayout();

    const auto scene = cw::gltf::Loader::loadGltf(gltfPath, options);
    REQUIRE_FALSE(scene.textures.isEmpty());
    const auto& texture = scene.textures.at(0);
    REQUIRE_FALSE(texture.pixels.isEmpty());

    SECTION("toImage borrows the texture's pixel buffer") {
        const QImage image = texture.toImage();
        REQUIRE_FALSE(image.isNull());
        REQUIRE(image.width() == texture.width);
        REQUIRE(image.height() == texture.height);
        REQUIRE(reinterpret_cast<const char*>(image.constBits()) == texture.pixels.constData());
    }

    SECTION("two images of the same texture share one pixel buffer") {
        const QImage first = texture.toImage();
        const QImage second = texture.toImage();
        REQUIRE(first.constBits() == second.constBits());
    }
}

namespace {

constexpr int kCompressedTextureSize = 32;
constexpr int kColorChannelMax = 255;

//Restores the format the render backend published, so a test that turns the
//compressed path on leaves the rest of the suite where it found it.
class SupportedFormatOverride
{
public:
    explicit SupportedFormatOverride(QRhiTexture::Format format) :
        m_previous(cw::ktx2::supportedCompressedFormat())
    {
        cw::ktx2::setSupportedCompressedFormat(format);
    }

    ~SupportedFormatOverride()
    {
        cw::ktx2::setSupportedCompressedFormat(m_previous);
    }

private:
    QRhiTexture::Format m_previous;
};

QImage compressibleImage()
{
    QImage image(kCompressedTextureSize, kCompressedTextureSize, QImage::Format_RGBA8888);
    for(int y = 0; y < image.height(); y++) {
        for(int x = 0; x < image.width(); x++) {
            const int red = x * kColorChannelMax / image.width();
            const int green = y * kColorChannelMax / image.height();
            image.setPixelColor(x, y, QColor(red, green, kColorChannelMax));
        }
    }
    return image;
}

bool sameTexture(const cwCompressedTexture& first, const cwCompressedTexture& second)
{
    return first.format == second.format
           && first.size == second.size
           && first.mipLevels == second.mipLevels;
}

cwDiskCacher::Key compressedTestKey(const QDir& dataRootDir)
{
    return cwDiskCacher::Key {
        QStringLiteral("gltf-texture-uastc.ktx2"),
        dataRootDir,
        QStringLiteral("checksum")
    };
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

cw::gltf::SceneCPU sceneWithBaseColor(const QImage& image)
{
    const QImage rgbaImage = image.convertToFormat(QImage::Format_RGBA8888);

    cw::gltf::TextureCPU texture;
    texture.width = rgbaImage.width();
    texture.height = rgbaImage.height();
    texture.isSRGB = true;
    texture.pixels = QByteArray(reinterpret_cast<const char*>(rgbaImage.constBits()),
                                rgbaImage.sizeInBytes());

    cw::gltf::SceneCPU scene;
    scene.textures.append(texture);
    return scene;
}

}

TEST_CASE("cachedCompressedTexture reuses the cached encode", "[Gltf][cwGltfLoader]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QDir dataRootDir(tempDir.path());
    cwDiskCacher cacher(dataRootDir);
    const cwDiskCacher::Key key = compressedTestKey(dataRootDir);
    const QRhiTexture::Format target = cw::ktx2::targetCompressedFormat();

    const auto first = cw::ktx2::cachedCompressedTexture(cacher, key, compressibleImage(), target);
    REQUIRE_FALSE(first.hasError());
    CHECK_FALSE(first.value().isNull());
    CHECK(first.value().size == QSize(kCompressedTextureSize, kCompressedTextureSize));
    CHECK(first.value().format == target);

    const QFileInfo cacheFile(cacher.filePath(key));
    REQUIRE(cacheFile.exists());
    const QDateTime firstWrite = cacheFile.lastModified();

    const auto second = cw::ktx2::cachedCompressedTexture(cacher, key, compressibleImage(), target);
    REQUIRE_FALSE(second.hasError());
    CHECK(sameTexture(first.value(), second.value()));
    CHECK(QFileInfo(cacher.filePath(key)).lastModified() == firstWrite);

    SECTION("a damaged entry re-encodes instead of failing") {
        QFile file(cacher.filePath(key));
        REQUIRE(file.open(QIODevice::ReadOnly));
        const QByteArray entryBytes = file.readAll();
        file.close();

        REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(entryBytes.left(entryBytes.size() / 2));
        file.close();

        const auto repaired = cw::ktx2::cachedCompressedTexture(cacher, key, compressibleImage(), target);
        REQUIRE_FALSE(repaired.hasError());
        CHECK(sameTexture(first.value(), repaired.value()));
    }

    SECTION("a missing entry without a source image is an error") {
        const cwDiskCacher::Key missingKey {
            QStringLiteral("missing-uastc.ktx2"),
            dataRootDir,
            QStringLiteral("checksum")
        };

        const auto missing = cw::ktx2::cachedCompressedTexture(cacher, missingKey, QImage(), target);
        CHECK(missing.hasError());
    }
}

TEST_CASE("glTF base color textures come from the KTX2 disk cache", "[Gltf][cwGltfLoader]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QDir dataRootDir(tempDir.path());
    const QString gltfPath = writeGltfFile(dataRootDir, QByteArray("glb-bytes"));
    REQUIRE_FALSE(gltfPath.isEmpty());

    const cw::gltf::SceneCPU scene = sceneWithBaseColor(compressibleImage());
    cw::gltf::MaterialCPU material;
    material.baseColorTextureIndex = 0;

    SECTION("the item takes the QImage while the backend's format is unknown") {
        const SupportedFormatOverride formatOverride(QRhiTexture::UnknownFormat);

        const cwGltfBaseColorTexture baseColorTexture(dataRootDir.path(), gltfPath);
        cwRenderTexturedItems::Item item;
        baseColorTexture.setOn(item, scene, material);

        CHECK_FALSE(item.texture.isNull());
        CHECK(item.compressedTexture.isNull());
    }

    SECTION("the second build hits the cache the first one filled") {
        const SupportedFormatOverride formatOverride(cw::ktx2::targetCompressedFormat());

        const cwGltfBaseColorTexture baseColorTexture(dataRootDir.path(), gltfPath);

        cwRenderTexturedItems::Item first;
        baseColorTexture.setOn(first, scene, material);
        REQUIRE_FALSE(first.compressedTexture.isNull());
        CHECK(first.compressedTexture.format == cw::ktx2::targetCompressedFormat());
        CHECK(first.compressedTexture.size == QSize(kCompressedTextureSize, kCompressedTextureSize));
        CHECK(first.texture.isNull());

        const cwDiskCacher cacher(dataRootDir);
        const cwDiskCacher::Key key {
            QStringLiteral("scan.glb-texture0-uastc.ktx2"),
            QFileInfo(gltfPath).dir(),
            QString()
        };
        const QFileInfo cacheFile(cacher.filePath(key));
        REQUIRE(cacheFile.exists());
        CHECK(cacheFile.absoluteFilePath().contains(QStringLiteral("/.cw_cache/")));
        const QDateTime firstWrite = cacheFile.lastModified();

        cwRenderTexturedItems::Item second;
        baseColorTexture.setOn(second, scene, material);
        CHECK(sameTexture(first.compressedTexture, second.compressedTexture));
        CHECK(QFileInfo(cacher.filePath(key)).lastModified() == firstWrite);
    }

    SECTION("an edited scan re-encodes instead of trusting the stale entry") {
        const SupportedFormatOverride formatOverride(cw::ktx2::targetCompressedFormat());

        const cwGltfBaseColorTexture original(dataRootDir.path(), gltfPath);
        cwRenderTexturedItems::Item first;
        original.setOn(first, scene, material);
        REQUIRE_FALSE(first.compressedTexture.isNull());

        REQUIRE_FALSE(writeGltfFile(dataRootDir, QByteArray("different-glb-bytes")).isEmpty());

        const cwGltfBaseColorTexture edited(dataRootDir.path(), gltfPath);
        cwRenderTexturedItems::Item second;
        edited.setOn(second, scene, material);
        CHECK_FALSE(second.compressedTexture.isNull());
    }
}
