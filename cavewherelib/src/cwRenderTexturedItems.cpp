#include "cwRenderTexturedItems.h"
#include "cwGeometryItersecter.h"
#include "cwRhiTexturedItems.h"
#include "Monad/Result.h"

#include <QtGlobal>

namespace {
Monad::Result<cwGeometry> geometryMatchesLayout(const cwGeometry& geometry,
                                                const QVector<cwGeometry::AttributeDesc>& layout)
{
    if (geometry.isEmpty()) {
        return Monad::Result<cwGeometry>(geometry);
    }

    if (layout.isEmpty()) {
        return Monad::Result<cwGeometry>(QStringLiteral("Expected layout is empty"));
    }

    const auto& attributes = geometry.attributes();
    if (attributes.size() != layout.size()) {
        return Monad::Result<cwGeometry>(QStringLiteral("Attribute count mismatch (expected %1, got %2)")
                                             .arg(layout.size())
                                             .arg(attributes.size()));
    }

    int expectedOffset = 0;
    for (int i = 0; i < layout.size(); ++i) {
        const auto& expected = layout[i];
        const auto& actual = attributes[i];
        if (actual.semantic != expected.semantic || actual.format != expected.format) {
            return Monad::Result<cwGeometry>(QStringLiteral("Attribute %1 mismatch").arg(i));
        }
        if (actual.byteOffset != expectedOffset) {
            return Monad::Result<cwGeometry>(QStringLiteral("Attribute %1 offset mismatch").arg(i));
        }
        expectedOffset += actual.byteSize();
    }

    if (geometry.vertexStride() != expectedOffset) {
        return Monad::Result<cwGeometry>(QStringLiteral("Vertex stride mismatch (expected %1, got %2)")
                                             .arg(expectedOffset)
                                             .arg(geometry.vertexStride()));
    }

    if (geometry.vertexStride() <= 0) {
        return Monad::Result<cwGeometry>(QStringLiteral("Invalid vertex stride"));
    }

    if (geometry.vertexData().size() % geometry.vertexStride() != 0) {
        return Monad::Result<cwGeometry>(QStringLiteral("Vertex buffer size is not a multiple of stride"));
    }

    const int vertexCount = geometry.vertexCount();
    for (uint32_t index : geometry.indices()) {
        if (index >= static_cast<uint32_t>(vertexCount)) {
            return Monad::Result<cwGeometry>(QStringLiteral("Index out of range (index %1, vertexCount %2)")
                                                 .arg(index)
                                                 .arg(vertexCount));
        }
    }

    return Monad::Result<cwGeometry>(geometry);


}

cwGeometry handleGeometryError(const Monad::Result<cwGeometry>& geometryResult) {
    if (geometryResult.hasError()) {
        qWarning().noquote() << "cwRenderTexturedItems: rejecting geometry with incompatible layout:"
                             << geometryResult.errorMessage();
    }
    return geometryResult.value();
}

Monad::Result<cwGeometry> geometryForRender(const cwGeometry& geometry)
{
    const auto layout = cwRenderTexturedItems::geometryLayout();
    return geometryMatchesLayout(geometry, layout);
}
}

cwRenderTexturedItems::cwRenderTexturedItems(QObject *parent) : cwRenderObject(parent) {}

QVector<cwGeometry::AttributeDesc> cwRenderTexturedItems::geometryLayout()
{
    return {
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 },
        { cwGeometry::Semantic::TexCoord0, cwGeometry::AttributeFormat::Vec2 }
    };
}

cwRHIObject* cwRenderTexturedItems::createRHIObject()
{
    // Ported behavior: match cwRenderScraps which returned a cwRhiScraps.
    // This lets the same RHI path draw textured geometry items.
    return new cwRhiTexturedItems();
}

void cwRenderTexturedItems::addCommand(const PendingCommand&& command)
{
    m_pendingChanges.append(command);
    update(); // schedule a render sync just like cwRenderScraps
}

uint32_t cwRenderTexturedItems::addItem(const Item& item)
{
    const uint32_t id = m_nextId++;
    Item commandItem = item;
    commandItem.geometry = handleGeometryError(geometryForRender(commandItem.geometry));

    addCommand(PendingCommand(PendingCommand::Add, id, commandItem));

    Item storedItem = item;
    if (!storedItem.storeGeometry) {
        storedItem.geometry = cwGeometry();
    }
    if (!storedItem.storeTexture) {
        storedItem.texture = QImage();
    }
    m_frontState.insert(id, storedItem);

    if (!commandItem.geometry.isEmpty()) {
        if (auto* intersector = geometryItersecter()) {
            intersector->addObject(cwGeometryItersecter::Object({this, id}, commandItem.geometry, item.modelMatrix));
        }
    }

    return id;
}

void cwRenderTexturedItems::updateGeometry(uint32_t id, const cwGeometry& geometry)
{
    auto entry = m_frontState.find(id);
    if (entry == m_frontState.end()) {
        return;
    }

    Item payload;
    payload.geometry = handleGeometryError(geometryForRender(geometry));
    addCommand(PendingCommand(PendingCommand::UpdateGeometry, id, payload));

    const QMatrix4x4 modelMatrix = entry->modelMatrix;
    if (entry->storeGeometry) {
        entry->geometry = payload.geometry;
    } else {
        entry->geometry = cwGeometry();
    }

    if (auto* intersector = geometryItersecter()) {
        if (!payload.geometry.isEmpty()) {
            intersector->addObject(cwGeometryItersecter::Object({this, id}, payload.geometry, modelMatrix));
        } else {
            intersector->removeObject({this, id});
        }
    }
}

void cwRenderTexturedItems::updateTexture(uint32_t id, const QImage& image)
{
    auto entry = m_frontState.find(id);
    if (entry == m_frontState.end()) {
        return;
    }

    Item payload;
    payload.texture = image; // geometry left default
    addCommand(PendingCommand(PendingCommand::UpdateTexture, id, payload));

    if (entry->storeTexture) {
        entry->texture = image;
    } else {
        entry->texture = QImage();
    }
}

void cwRenderTexturedItems::setVisible(uint32_t id, bool visible)
{
    auto entry = m_frontState.find(id);
    if (entry == m_frontState.end()) {
        return;
    }

    Item payload;
    payload.visible = visible;
    addCommand(PendingCommand(PendingCommand::UpdateVisiblity, id, payload));

    entry->visible = visible;
}

void cwRenderTexturedItems::setCulling(uint32_t id, CullMode culling)
{
    auto entry = m_frontState.find(id);
    if (entry == m_frontState.end()) {
        return;
    }

    entry->material.cullMode = culling;

    Item payload;
    payload.material = entry->material;
    addCommand(PendingCommand(PendingCommand::UpdateMaterial, id, payload));
}

void cwRenderTexturedItems::setMaterial(uint32_t id, const cwRenderMaterialState& material)
{
    auto entry = m_frontState.find(id);
    if (entry == m_frontState.end()) {
        return;
    }

    Item payload;
    payload.material = material;
    addCommand(PendingCommand(PendingCommand::UpdateMaterial, id, payload));

    entry->material = material;
}

void cwRenderTexturedItems::setUniformBlock(uint32_t id, const QByteArray& uniformBlock)
{
    auto entry = m_frontState.find(id);
    if (entry == m_frontState.end()) {
        return;
    }

    Item payload;
    payload.uniformBlock = uniformBlock;
    addCommand(PendingCommand(PendingCommand::UpdateUniformBlock, id, payload));

    entry->uniformBlock = uniformBlock;
}

void cwRenderTexturedItems::setModelMatrix(uint32_t id, const QMatrix4x4& modelMatrix)
{
    auto entry = m_frontState.find(id);
    if (entry == m_frontState.end()) {
        return;
    }

    Item payload;
    payload.modelMatrix = modelMatrix;
    addCommand(PendingCommand(PendingCommand::UpdateModelMatrix, id, payload));

    entry->modelMatrix = modelMatrix;

    if (auto* intersector = geometryItersecter()) {
        intersector->setModelMatrix({this, id}, modelMatrix);
    }
}

void cwRenderTexturedItems::removeItem(uint32_t id)
{
    if (!m_frontState.contains(id)) {
        return;
    }

    addCommand(PendingCommand(PendingCommand::Remove, id, Item{}));

    if (auto* intersector = geometryItersecter()) {
        intersector->removeObject({this, id});
    }
    m_frontState.remove(id);
}

cwRenderTexturedItems::Item cwRenderTexturedItems::item(uint32_t id) const
{
    return m_frontState.value(id);
}

bool cwRenderTexturedItems::hasItem(uint32_t id) const
{
    return m_frontState.contains(id);
}
