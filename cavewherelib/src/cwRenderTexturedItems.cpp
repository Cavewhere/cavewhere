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
    // Coalesce per id so the queue holds the *latest* state for each item
    // rather than a running log of every edit. Without this, re-triangulation
    // driven by repeated line-plot runs (e.g. stepping a trip's declination)
    // stacks a full-mesh cwGeometry payload per edit, and the queue is only
    // drained when the 3D view renders a frame — so editing while the view is
    // hidden grows memory without bound (issue #629).
    const uint32_t id = command.id();
    const auto type = command.type();

    const auto pendingIndexOfType = [this, id](PendingCommand::Type wanted) -> int {
        for (int i = 0; i < m_pendingChanges.size(); ++i) {
            const auto& pending = m_pendingChanges.at(i);
            if (pending.id() == id && pending.type() == wanted) {
                return i;
            }
        }
        return -1;
    };

    switch (type) {
    case PendingCommand::Remove: {
        const bool hadPendingAdd = pendingIndexOfType(PendingCommand::Add) >= 0;
        // Drop every still-pending command for this id: its updates are moot,
        // and a pending Add means the item never reached the render thread, so
        // the Add+Remove pair annihilates and no Remove need be recorded.
        m_pendingChanges.removeIf([id](const PendingCommand& pending) {
            return pending.id() == id;
        });
        if (!hadPendingAdd) {
            // The item was already synced to the render thread; record the
            // removal so the next sync tears it down.
            m_pendingChanges.append(command);
        }
        break;
    }
    case PendingCommand::UpdateGeometry:
    case PendingCommand::UpdateTexture:
    case PendingCommand::UpdateMaterial:
    case PendingCommand::UpdateUniformBlock:
    case PendingCommand::UpdateModelMatrix: {
        const int addIndex = pendingIndexOfType(PendingCommand::Add);
        if (addIndex >= 0) {
            // Fold the update into the not-yet-synced Add so its single payload
            // carries the latest state.
            m_pendingChanges[addIndex].mergeUpdatePayload(type, command.payload());
            break;
        }
        const int sameTypeIndex = pendingIndexOfType(type);
        if (sameTypeIndex >= 0) {
            // Replace the previous same-field update with the latest value.
            m_pendingChanges[sameTypeIndex] = command;
            break;
        }
        m_pendingChanges.append(command);
        break;
    }
    case PendingCommand::Add:
    case PendingCommand::Unknown:
        // Ids are unique and monotonic, so an Add never collides with a
        // pending command for the same id.
        m_pendingChanges.append(command);
        break;
    }

    update(); // schedule a render sync just like cwRenderScraps
}

uint32_t cwRenderTexturedItems::addItem(const Item& item)
{
    const uint32_t id = m_nextId++;
    ItemPayload payload;
    payload.geometry = handleGeometryError(geometryForRender(item.geometry));
    payload.texture = item.texture;
    payload.material = item.material;
    payload.uniformBlock = item.uniformBlock;
    payload.modelMatrix = item.modelMatrix;

    addCommand(PendingCommand(PendingCommand::Add, id, payload));

    Item storedItem = item;
    if (!storedItem.storeGeometry) {
        storedItem.geometry = cwGeometry();
    }
    if (!storedItem.storeTexture) {
        storedItem.texture = QImage();
    }
    m_frontState.insert(id, storedItem);

    if (!payload.geometry.isEmpty()) {
        registerPickable(id, payload.geometry, item.modelMatrix);
    } else {
        qCDebug(lcPick).nospace()
            << "addItem id=" << id
            << " inputGeometryEmpty=" << item.geometry.isEmpty()
            << " transformedGeometryEmpty=true"
            << " -> not registered for picking at addItem"
            << " (later updateGeometry(id, nonEmpty) would still register it)";
    }

    // Publish to the store (issue #579): the entry is identity state keyed by
    // (ownerId, id), independent of geometry registration — pick traversals
    // read it from a snapshot, so a born-hidden item is unpickable even
    // before (or without) its geometry registering. A visible item is the
    // sparse default, so this only writes when born hidden or gated (Phase 4).
    publishItemVisibility(id, item.visible);

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

    ItemPayload payload;
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

    if (!payload.geometry.isEmpty()) {
        // The store entry is identity state that survives geometry removal and
        // re-registration, so a render-hidden item can't be resurrected by a
        // geometry cycle. registerPickable is first-publish-only: it hides the
        // item only on its first registration (addItem had empty geometry) and
        // no-ops on a same-key replacement, so an already-shown item never
        // blinks on a geometry edit (Phase 4).
        registerPickable(id, payload.geometry, modelMatrix);
        publishItemVisibility(id, entry->visible);
    } else {
        // Geometry gone: drop it from picking and its gate, then republish so a
        // gate armed by an earlier non-empty registration doesn't leave the
        // item flagged hidden after the gate is gone.
        unregisterPickable(id);
        publishItemVisibility(id, entry->visible);
    }
}

void cwRenderTexturedItems::updateTexture(uint32_t id, const QImage& image)
{
    auto entry = m_frontState.find(id);
    if (entry == m_frontState.end()) {
        return;
    }

    ItemPayload payload;
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
    // the frame's snapshot. A user show while gated still yields hidden until
    // the sub-BVH publishes (Phase 4).
    publishItemVisibility(id, visible);
    update();
}

void cwRenderTexturedItems::setCulling(uint32_t id, CullMode culling)
{
    auto entry = m_frontState.find(id);
    if (entry == m_frontState.end()) {
        return;
    }

    entry->material.cullMode = culling;

    ItemPayload payload;
    payload.material = entry->material;
    addCommand(PendingCommand(PendingCommand::UpdateMaterial, id, payload));
}

void cwRenderTexturedItems::setMaterial(uint32_t id, const cwRenderMaterialState& material)
{
    auto entry = m_frontState.find(id);
    if (entry == m_frontState.end()) {
        return;
    }

    ItemPayload payload;
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

    ItemPayload payload;
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

    ItemPayload payload;
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

    addCommand(PendingCommand(PendingCommand::Remove, id, ItemPayload{}));

    unregisterPickable(id);
    if (auto* visibility = sceneVisibility()) {
        visibility->removeSub(renderObjectId(), id);
    }
    m_frontState.remove(id);
}

void cwRenderTexturedItems::updateVisibility()
{
    cwRenderObject::updateVisibility();
    for (auto it = m_frontState.cbegin(); it != m_frontState.cend(); ++it) {
        publishItemVisibility(it.key(), it.value().visible);
    }
}

void cwRenderTexturedItems::publishItemVisibility(uint32_t id, bool authoredVisible)
{
    if (auto* visibility = sceneVisibility()) {
        visibility->setSubVisible(renderObjectId(), id, authoredVisible && subPickGateOpen(id));
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
