#include <catch2/catch_test_macros.hpp>

#include <QColor>
#include <QImage>
#include <QVector2D>
#include <QVector3D>
#include <QVector>

#include "cwRenderTexturedItems.h"
#include "cwGeometry.h"

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
