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
    Item copy = item;
    m_frontState.insert(id, copy);
    addCommand(PendingCommand(PendingCommand::Add, id, copy));

    m_ids.insert(id);

    geometryItersecter()->addObject(cwGeometryItersecter::Object({this, id}, item.geometry, item.modelMatrix));

    return id;
}

void cwRenderTexturedItems::updateGeometry(uint32_t id, const cwGeometry& geometry)
{
    if(!m_ids.contains(id)) {
        return;
    }

    Item payload;
    payload.geometry = geometry; // texture left empty; RHI side should only consume what's needed
    addCommand(PendingCommand(PendingCommand::UpdateGeometry, id, payload));

    auto entry = m_frontState.find(id);
    if (entry != m_frontState.end()) {
        entry->geometry = geometry;
    }

    QMatrix4x4 modelMatrix = entry != m_frontState.end() ? entry->modelMatrix : QMatrix4x4();
    geometryItersecter()->addObject(cwGeometryItersecter::Object({this, id}, geometry, modelMatrix));
}

void cwRenderTexturedItems::updateTexture(uint32_t id, const QImage& image)
{
    if(!m_ids.contains(id)) {
        return;
    }

    Item payload;
    payload.texture = image; // geometry left default
    addCommand(PendingCommand(PendingCommand::UpdateTexture, id, payload));

    auto entry = m_frontState.find(id);
    if (entry != m_frontState.end()) {
        entry->texture = image;
    }
}

void cwRenderTexturedItems::setVisible(uint32_t id, bool visible)
{
    if(!m_ids.contains(id)) {
        return;
    }

    Item payload;
    payload.visible = visible;
    addCommand(PendingCommand(PendingCommand::UpdateVisiblity, id, payload));

    auto entry = m_frontState.find(id);
    if (entry != m_frontState.end()) {
        entry->visible = visible;
    }
}

void cwRenderTexturedItems::setCulling(uint32_t id, CullMode culling)
{
    if(!m_ids.contains(id)) {
        return;
    }

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
    if(!m_ids.contains(id)) {
        return;
    }

    Item payload;
    payload.material = material;
    addCommand(PendingCommand(PendingCommand::UpdateMaterial, id, payload));

    m_frontState[id].material = material;
}

void cwRenderTexturedItems::setUniformBlock(uint32_t id, const QByteArray& uniformBlock)
{
    if(!m_ids.contains(id)) {
        return;
    }

    Item payload;
    payload.uniformBlock = uniformBlock;
    addCommand(PendingCommand(PendingCommand::UpdateUniformBlock, id, payload));

    auto entry = m_frontState.find(id);
    if (entry != m_frontState.end()) {
        entry->uniformBlock = uniformBlock;
    }
}

void cwRenderTexturedItems::setModelMatrix(uint32_t id, const QMatrix4x4& modelMatrix)
{
    if (!m_ids.contains(id)) {
        return;
    }

    Item payload;
    payload.modelMatrix = modelMatrix;
    addCommand(PendingCommand(PendingCommand::UpdateModelMatrix, id, payload));

    auto entry = m_frontState.find(id);
    if (entry != m_frontState.end()) {
        entry->modelMatrix = modelMatrix;
    }

    geometryItersecter()->setModelMatrix({this, id}, modelMatrix);
}

void cwRenderTexturedItems::removeItem(uint32_t id)
{
    if(!m_ids.contains(id)) {
        return;
    }

    addCommand(PendingCommand(PendingCommand::Remove, id, Item{}));

    geometryItersecter()->removeObject({this, id});
    m_ids.remove(id);
    m_frontState.remove(id);
}
