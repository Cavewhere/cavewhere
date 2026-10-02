#include "cwRenderTexturedItemVisibility.h"

#include "cwRenderTexturedItems.h"

cwRenderTexturedItemVisibility::cwRenderTexturedItemVisibility(cwRenderTexturedItems *items,
                                                               uint32_t itemId,
                                                               QObject *parent)
    : cwVisibilityProxy(parent),
      m_items(items),
      m_itemId(itemId)
{
}

void cwRenderTexturedItemVisibility::applyVisible(bool visible)
{
    if(m_items) {
        m_items->setItemVisible(m_itemId, visible);
    }
}
