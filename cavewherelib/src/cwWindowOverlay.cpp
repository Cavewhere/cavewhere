/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwWindowOverlay.h"
#include "cwWindowOverlayAttachedType.h"
#include "cwWindowOverlayRegistry.h"

cwWindowOverlay::cwWindowOverlay(QQuickItem* parentItem) :
    QQuickItem(parentItem)
{
}

cwWindowOverlayAttachedType* cwWindowOverlay::qmlAttachedProperties(QObject* object)
{
    return new cwWindowOverlayAttachedType(object);
}

cwWindowOverlay::~cwWindowOverlay()
{
    cwWindowOverlayRegistry::instance()->removeOverlay(m_registeredWindow, this);
}

void cwWindowOverlay::itemChange(ItemChange change, const ItemChangeData& data)
{
    if(change == ItemSceneChange) {
        auto registry = cwWindowOverlayRegistry::instance();

        registry->removeOverlay(m_registeredWindow, this);
        m_registeredWindow = data.window;
        registry->addOverlay(m_registeredWindow, this);
    }

    QQuickItem::itemChange(change, data);
}
