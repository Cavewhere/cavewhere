#include "cwRenderTexturedItemVisibility.h"

#include "cwRenderTexturedItems.h"

cwRenderTexturedItemVisibility::cwRenderTexturedItemVisibility(cwRenderTexturedItems *items,
                                                               uint32_t itemId,
                                                               QObject *parent)
    : QObject(parent),
      m_items(items),
      m_itemId(itemId)
{
}

void cwRenderTexturedItemVisibility::setVisible(bool visible)
{
    if(m_visible == visible) {
        return;
    }

    m_visible = visible;
    if(m_items) {
        m_items->setVisible(m_itemId, m_visible);
    }

    emit visibleChanged();
}
