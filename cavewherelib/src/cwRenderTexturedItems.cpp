#include "cwRenderTexturedItems.h"
#include "cwGeometryItersecter.h"
#include "cwPickingLog.h"
#include "cwRhiTexturedItems.h"
#include "cwSceneVisibility.h"
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

    // cwRhiTexturedItems expects a single interleaved vertex buffer.
    if (geometry.layoutMode() != cwGeometry::LayoutMode::Interleaved) {
        return Monad::Result<cwGeometry>(QStringLiteral("Geometry must use Interleaved layout"));
    }

    int expectedOffset = 0;
    for (int i = 0; i < layout.size(); ++i) {
        const auto& expected = layout[i];
        const auto& actual = attributes[i];
        if (actual.semantic != expected.semantic || actual.format != expected.format) {
            return Monad::Result<cwGeometry>(QStringLiteral("Attribute %1 mismatch").arg(i));
        }
        if (actual.byteOffsetInBuffer != expectedOffset) {
            return Monad::Result<cwGeometry>(QStringLiteral("Attribute %1 offset mismatch").arg(i));
        }
        expectedOffset += actual.byteSize();
    }

    const auto buffers = geometry.vertexBuffers();
    if (buffers.size() != 1) {
        return Monad::Result<cwGeometry>(QStringLiteral("Expected 1 vertex buffer, got %1")
                                             .arg(buffers.size()));
    }
    const int stride = buffers[0].stride;
    if (stride != expectedOffset) {
        return Monad::Result<cwGeometry>(QStringLiteral("Vertex stride mismatch (expected %1, got %2)")
                                             .arg(expectedOffset)
                                             .arg(stride));
    }

    if (stride <= 0) {
        return Monad::Result<cwGeometry>(QStringLiteral("Invalid vertex stride"));
    }

    if (buffers[0].data->size() % stride != 0) {
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
        qCWarning(lcPick).noquote()
            << "cwRenderTexturedItems: rejecting geometry with incompatible layout:"
            << geometryResult.errorMessage()
            << "-> returning empty geometry (picker.addObject will be SKIPPED)";
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
            intersector->addObject(cwGeometryItersecter::Object(this, id, commandItem.geometry, item.modelMatrix));
        } else {
            qCDebug(lcPick).nospace()
                << "addItem id=" << id
                << " geometry non-empty but geometryItersecter()==nullptr"
                << " (scene wiring not ready) - picker WILL NOT see this item";
        }
    } else {
        qCDebug(lcPick).nospace()
            << "addItem id=" << id
            << " inputGeometryEmpty=" << item.geometry.isEmpty()
            << " transformedGeometryEmpty=true"
            << " -> picker.addObject SKIPPED at addItem"
            << " (later updateGeometry(id, nonEmpty) would still register it)";
    }

    // Publish to the store (issue #579): the entry is identity state keyed by
    // (ownerId, id), independent of geometry registration — pick traversals
    // read it from a snapshot, so a born-hidden item is unpickable even
    // before (or without) its geometry registering. A visible item is the
    // sparse default, so this only writes when born hidden.
    if (auto* visibility = sceneVisibility()) {
        visibility->setSubVisible(renderObjectId(), id, item.visible);
    }

    return id;
}

void cwRenderTexturedItems::updateGeometry(uint32_t id, const cwGeometry& geometry)
{
    auto entry = m_frontState.find(id);
    if (entry == m_frontState.end()) {
        qCDebug(lcPick).nospace()
            << "updateGeometry id=" << id
            << " IGNORED: id not in m_frontState"
            << " (addItem was never called for this id, or it was already removed)";
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

    auto* intersector = geometryItersecter();
    qCDebug(lcPick).nospace()
        << "updateGeometry id=" << id
        << " inputVertexCount=" << geometry.vertexCount()
        << " inputIndexCount=" << geometry.indices().size()
        << " inputType=" << cwGeometry::typeName(geometry.type())
        << " inputLayoutMode=" << static_cast<int>(geometry.layoutMode())
        << " transformedEmpty=" << payload.geometry.isEmpty()
        << " hasIntersector=" << (intersector != nullptr)
        << " action="
        << (intersector == nullptr ? "no-intersector"
            : payload.geometry.isEmpty() ? "removeObject"
            : "addObject");

    if (intersector != nullptr) {
        if (!payload.geometry.isEmpty()) {
            // No visibility re-seed needed: the store entry is identity
            // state that survives geometry removal and re-registration, so a
            // render-hidden item can't be resurrected by a geometry cycle.
            intersector->addObject(cwGeometryItersecter::Object(this, id, payload.geometry, modelMatrix));
        } else {
            intersector->removeObject({renderObjectId(), id});
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

void cwRenderTexturedItems::setItemVisible(uint32_t id, bool visible)
{
    auto entry = m_frontState.find(id);
    if (entry == m_frontState.end()) {
        return;
    }

    entry->visible = visible;

    // The store entry is the single published truth: the intersecter reads it
    // from a snapshot per query (a hidden item must not take picks or inflate
    // the reset-view bounds, issues #575/#549), and cwRhiTexturedItems::gather
    // reads it from the frame's snapshot — no per-item visibility command
    // travels to the render thread. update() schedules the sync that refreshes
    // the frame's snapshot.
    if (auto* visibilityStore = sceneVisibility()) {
        visibilityStore->setSubVisible(renderObjectId(), id, visible);
    }
    update();
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
        intersector->setModelMatrix({renderObjectId(), id}, modelMatrix);
    }
}

void cwRenderTexturedItems::removeItem(uint32_t id)
{
    if (!m_frontState.contains(id)) {
        return;
    }

    addCommand(PendingCommand(PendingCommand::Remove, id, Item{}));

    if (auto* intersector = geometryItersecter()) {
        intersector->removeObject({renderObjectId(), id});
    }
    if (auto* visibility = sceneVisibility()) {
        visibility->removeSub(renderObjectId(), id);
    }
    m_frontState.remove(id);
}

void cwRenderTexturedItems::publishVisibility()
{
    cwRenderObject::publishVisibility();
    auto* visibility = sceneVisibility();
    if (visibility == nullptr) {
        return;
    }
    for (auto it = m_frontState.cbegin(); it != m_frontState.cend(); ++it) {
        visibility->setSubVisible(renderObjectId(), it.key(), it.value().visible);
    }
}

cwRenderTexturedItems::Item cwRenderTexturedItems::item(uint32_t id) const
{
    return m_frontState.value(id);
}

bool cwRenderTexturedItems::hasItem(uint32_t id) const
{
    return m_frontState.contains(id);
}
