/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwVisibilityProxy.h"

cwVisibilityProxy::cwVisibilityProxy(QObject* parent) :
    QObject(parent)
{
}

void cwVisibilityProxy::setVisible(bool visible)
{
    if(m_visible == visible) {
        return;
    }

    m_visible = visible;
    applyVisible(visible);
    emit visibleChanged();
}
