#include "cwRenderTexturedItems.h"
#include "cwGeometryItersecter.h"
#include "cwRhiTexturedItems.h"

cwRenderTexturedItems::cwRenderTexturedItems(QObject *parent) : cwRenderObject(parent) {}

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
    addCommand(PendingCommand(PendingCommand::Add, id, commandItem));

    Item storedItem = item;
    if (!storedItem.storeGeometry) {
        storedItem.geometry = cwGeometry();
    }
    if (!storedItem.storeTexture) {
        storedItem.texture = QImage();
    }
    m_frontState.insert(id, storedItem);

    if (auto* intersector = geometryItersecter()) {
        intersector->addObject(cwGeometryItersecter::Object({this, id}, item.geometry, item.modelMatrix));
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
    payload.geometry = geometry; // texture left empty; RHI side should only consume what's needed
    addCommand(PendingCommand(PendingCommand::UpdateGeometry, id, payload));

    const QMatrix4x4 modelMatrix = entry->modelMatrix;
    if (entry->storeGeometry) {
        entry->geometry = geometry;
    } else {
        entry->geometry = cwGeometry();
    }

    if (auto* intersector = geometryItersecter()) {
        intersector->addObject(cwGeometryItersecter::Object({this, id}, geometry, modelMatrix));
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
