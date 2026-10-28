/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwMarkerVisibility.h"
#include "cwMarkerView.h"

cwMarkerVisibility::cwMarkerVisibility(cwMarkerView* view, const QString& group, QObject* parent)
    : cwVisibilityProxy(parent),
      m_view(view),
      m_group(group)
{
}

void cwMarkerVisibility::applyVisible(bool visible)
{
    if(!m_view.isNull()) {
        m_view->setGroupKeywordVisible(m_group, visible);
    }
}
