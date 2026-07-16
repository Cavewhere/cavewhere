/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLeadVisibility.h"
#include "cwLeadView.h"
#include "cwScrap.h"

cwLeadVisibility::cwLeadVisibility(cwLeadView* view, cwScrap* scrap, QObject* parent)
    : QObject(parent),
      m_view(view),
      m_scrap(scrap)
{
}

void cwLeadVisibility::setVisible(bool visible)
{
    if(m_visible == visible) {
        return;
    }

    m_visible = visible;
    if(!m_view.isNull() && !m_scrap.isNull()) {
        m_view->setScrapKeywordVisible(m_scrap, visible);
    }

    emit visibleChanged();
}
