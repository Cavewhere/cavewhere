#include "cwRenderTexturedItemsVisibilityGroup.h"

#include "cwRenderTexturedItems.h"

cwRenderTexturedItemsVisibilityGroup::cwRenderTexturedItemsVisibilityGroup(cwRenderTexturedItems *items,
                                                                         QVector<uint32_t> ids,
                                                                         QObject *parent) :
    QObject(parent),
    m_items(items),
    m_ids(std::move(ids))
{
}

void cwRenderTexturedItemsVisibilityGroup::setVisible(bool visible)
{
    if(m_visible == visible || !m_items) {
        return;
    }

    m_visible = visible;
    for(auto id : m_ids) {
        m_items->setVisible(id, visible);
    }
    emit visibleChanged();
}
