#include <catch2/catch_test_macros.hpp>

#include <QColor>
#include <QImage>
#include <QVector2D>
#include <QVector3D>
#include <QVector>

#include "cwRenderTexturedItems.h"
#include "cwGeometry.h"
#include "cwStreamedTexture.h"

namespace {

cwGeometry makeGeometry(int vertexCount, float offset)
{
    cwGeometry geometry({
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 },
        { cwGeometry::Semantic::TexCoord0, cwGeometry::AttributeFormat::Vec2 }
    });

    geometry.resizeVertices(vertexCount);
    const auto* positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
    const auto* texCoordAttribute = geometry.attribute(cwGeometry::Semantic::TexCoord0);
    for (int i = 0; i < vertexCount; ++i) {
        geometry.set(positionAttribute, i, QVector3D(offset + float(i), offset, float(i)));
        geometry.set(texCoordAttribute, i, QVector2D(float(i), offset));
    }

    QVector<uint32_t> indices;
    if (vertexCount >= 3) {
        for (int i = 1; i < vertexCount - 1; ++i) {
            indices << 0u << uint32_t(i) << uint32_t(i + 1);
        }
    }
    geometry.setIndices(std::move(indices));

    return geometry;
}

QImage makeImage(const QColor& color)
{
    QImage image(2, 2, QImage::Format_ARGB32);
    image.fill(color);
    return image;
}

cwStreamedTexture makeStreamedTexture(const QString& id, const QSize& size)
{
    cwStreamedTexture streamed;
    streamed.dataRootPath = QStringLiteral("/data/root");
    streamed.key.id = id;
    streamed.key.path = QStringLiteral("textures");
    streamed.key.checksum = QStringLiteral("checksum-") + id;
    streamed.size = size;
    return streamed;
}

} // namespace

TEST_CASE("cwRenderTexturedItems storage flags control CPU data retention", "[cwRenderTexturedItems]")
{
    cwRenderTexturedItems render;
    const QImage initialImage = makeImage(Qt::red);
    const QImage updatedImage = makeImage(Qt::blue);

    SECTION("defaults drop geometry and texture after upload")
    {
        cwRenderTexturedItems::Item item;
        item.geometry = makeGeometry(3, 0.0f);
        item.texture = initialImage;

        const uint32_t id = render.addItem(item);
        REQUIRE(render.hasItem(id));

        auto stored = render.item(id);
        REQUIRE_FALSE(stored.storeGeometry);
        REQUIRE_FALSE(stored.storeTexture);
        REQUIRE(stored.geometry.vertexCount() == 0);
        REQUIRE(stored.geometry.indices().isEmpty());
        REQUIRE(stored.texture.isNull());

        cwGeometry updatedGeometry = makeGeometry(4, 1.0f);
        render.updateGeometry(id, updatedGeometry);
        stored = render.item(id);
        REQUIRE(stored.geometry.vertexCount() == 0);
        REQUIRE(stored.geometry.indices().isEmpty());

        render.updateTexture(id, updatedImage);
        stored = render.item(id);
        REQUIRE(stored.texture.isNull());
    }

    SECTION("opt-in flags retain geometry and texture data")
    {
        cwRenderTexturedItems::Item item;
        item.geometry = makeGeometry(3, 5.0f);
        item.texture = initialImage;
        item.storeGeometry = true;
        item.storeTexture = true;

        const uint32_t id = render.addItem(item);
        REQUIRE(render.hasItem(id));

        auto stored = render.item(id);
        REQUIRE(stored.storeGeometry);
        REQUIRE(stored.storeTexture);
        REQUIRE(stored.geometry.vertexCount() == item.geometry.vertexCount());
        REQUIRE(stored.geometry.indices() == item.geometry.indices());
        REQUIRE_FALSE(stored.texture.isNull());
        REQUIRE(stored.texture.pixelColor(0, 0) == initialImage.pixelColor(0, 0));

        cwGeometry updatedGeometry = makeGeometry(4, 10.0f);
        render.updateGeometry(id, updatedGeometry);
        stored = render.item(id);
        REQUIRE(stored.geometry.vertexCount() == updatedGeometry.vertexCount());
        REQUIRE(stored.geometry.indices() == updatedGeometry.indices());
        REQUIRE(
            stored.geometry.value<QVector3D>(cwGeometry::Semantic::Position, 0)
            == updatedGeometry.value<QVector3D>(cwGeometry::Semantic::Position, 0));

        render.updateTexture(id, updatedImage);
        stored = render.item(id);
        REQUIRE_FALSE(stored.texture.isNull());
        REQUIRE(stored.texture.pixelColor(0, 0) == updatedImage.pixelColor(0, 0));
    }
}

TEST_CASE("cwRenderTexturedItems carries a streamed source instead of pixels",
          "[TexturedItemsStreaming]")
{
    cwRenderTexturedItems render;
    const QImage image = makeImage(Qt::green);
    const cwStreamedTexture firstStreamed =
        makeStreamedTexture(QStringLiteral("scrap-1"), QSize(2048, 2048));
    const cwStreamedTexture secondStreamed =
        makeStreamedTexture(QStringLiteral("scrap-2"), QSize(1024, 1024));

    SECTION("an item added with only a streamed source keeps its descriptor")
    {
        cwRenderTexturedItems::Item item;
        item.geometry = makeGeometry(3, 0.0f);
        item.streamedTexture = firstStreamed;

        const uint32_t id = render.addItem(item);
        const auto stored = render.item(id);
        // The descriptor is kept whatever storeTexture says — it is a handful of
        // strings, and updateStreamedTexture compares against it.
        REQUIRE_FALSE(stored.storeTexture);
        CHECK(stored.streamedTexture == firstStreamed);
        CHECK(stored.texture.isNull());
    }

    SECTION("a streamed source wins when an item is added with both")
    {
        cwRenderTexturedItems::Item item;
        item.geometry = makeGeometry(3, 0.0f);
        item.texture = image;
        item.streamedTexture = firstStreamed;
        item.storeTexture = true;

        const uint32_t id = render.addItem(item);
        const auto stored = render.item(id);
        CHECK(stored.streamedTexture == firstStreamed);
        CHECK(stored.texture.isNull());
    }

    SECTION("each representation clears the other")
    {
        cwRenderTexturedItems::Item item;
        item.geometry = makeGeometry(3, 0.0f);
        item.texture = image;
        item.storeTexture = true;

        const uint32_t id = render.addItem(item);
        REQUIRE_FALSE(render.item(id).texture.isNull());

        render.updateStreamedTexture(id, firstStreamed);
        auto stored = render.item(id);
        CHECK(stored.streamedTexture == firstStreamed);
        CHECK(stored.texture.isNull());

        render.updateTexture(id, image);
        stored = render.item(id);
        CHECK(stored.streamedTexture.isNull());
        CHECK_FALSE(stored.texture.isNull());

        render.updateStreamedTexture(id, secondStreamed);
        stored = render.item(id);
        CHECK(stored.streamedTexture == secondStreamed);
        CHECK(stored.texture.isNull());
    }

    SECTION("repeated streamed updates coalesce with the last one winning")
    {
        cwRenderTexturedItems::Item item;
        item.geometry = makeGeometry(3, 0.0f);

        const uint32_t id = render.addItem(item);
        render.updateStreamedTexture(id, firstStreamed);
        render.updateStreamedTexture(id, secondStreamed);

        CHECK(render.item(id).streamedTexture == secondStreamed);
    }

    SECTION("updateItem routes the streamed representation")
    {
        cwRenderTexturedItems::Item item;
        item.geometry = makeGeometry(3, 0.0f);
        item.texture = image;
        item.storeTexture = true;

        const uint32_t id = render.addItem(item);

        cwRenderTexturedItems::Item updated = item;
        updated.texture = image;
        updated.streamedTexture = firstStreamed;
        render.updateItem(id, updated);

        const auto stored = render.item(id);
        CHECK(stored.streamedTexture == firstStreamed);
        CHECK(stored.texture.isNull());
    }
}
