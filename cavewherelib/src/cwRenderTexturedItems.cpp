#include "cwRenderTexturedItems.h"
#include "cwGeometryItersecter.h"
#include "cwRhiTexturedItems.h"

cwRenderTexturedItems::cwRenderTexturedItems() {}

cwRHIObject* cwRenderTexturedItems::createRHIObject()
{
    // Ported behavior: match cwRenderScraps which returned a cwRhiScraps.
    // This lets the same RHI path draw textured geometry items.
    return new cwRhiTexturedItems();
}

void cwRenderTexturedItems::addCommand(const PendingCommand& command)
{
    m_pendingChanges.append(command);
    update(); // schedule a render sync just like cwRenderScraps
}

uint32_t cwRenderTexturedItems::addItem(const Item& item)
{
    const uint32_t id = m_nextId++;
    addCommand(PendingCommand(PendingCommand::Add, id, item));

    m_ids.insert(id);

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

    geometryItersecter()->addObject(cwGeometryItersecter::Object({this, id}, geometry));
}

void cwRenderTexturedItems::updateTexture(uint32_t id, const QImage& image)
{
    if(!m_ids.contains(id)) {
        return;
    }

    Item payload;
    payload.texture = image; // geometry left default
    addCommand(PendingCommand(PendingCommand::UpdateTexture, id, payload));
}

void cwRenderTexturedItems::removeItem(uint32_t id)
{
    if(!m_ids.contains(id)) {
        return;
    }


    addCommand(PendingCommand(PendingCommand::Remove, id, Item{}));

    geometryItersecter()->removeObject({this, id});
    m_ids.remove(id);
}
