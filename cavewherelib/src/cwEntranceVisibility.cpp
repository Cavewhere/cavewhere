/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwEntranceVisibility.h"
#include "cwEntranceView.h"

cwEntranceVisibility::cwEntranceVisibility(cwEntranceView* view, const QUuid& fixId, QObject* parent)
    : cwVisibilityProxy(parent),
      m_view(view),
      m_fixId(fixId)
{
}

void cwEntranceVisibility::applyVisible(bool visible)
{
    if(!m_view.isNull()) {
        m_view->setFixKeywordVisible(m_fixId, visible);
    }
}
