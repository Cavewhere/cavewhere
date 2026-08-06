#include "cwRenderTexturedItemsVisibilityGroup.h"

#include "cwRenderTexturedItems.h"

cwRenderTexturedItemsVisibilityGroup::cwRenderTexturedItemsVisibilityGroup(cwRenderTexturedItems *items,
                                                                         QVector<uint32_t> ids,
                                                                         QObject *parent) :
    cwVisibilityProxy(parent),
    m_items(items),
    m_ids(std::move(ids))
{
}

void cwRenderTexturedItemsVisibilityGroup::applyVisible(bool visible)
{
    if(!m_items) {
        return;
    }

    for(auto id : m_ids) {
        m_items->setItemVisible(id, visible);
    }
}
