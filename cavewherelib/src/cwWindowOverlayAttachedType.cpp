/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Qt includes
#include <QDebug>
#include <QQuickItem>
#include <QQuickWindow>

//Our includes
#include "cwWindowOverlayAttachedType.h"
#include "cwWindowOverlay.h"
#include "cwWindowOverlayRegistry.h"

cwWindowOverlayAttachedType::cwWindowOverlayAttachedType(QObject* parent) :
    QObject(parent),
    m_item(qobject_cast<QQuickItem*>(parent)),
    m_window(qobject_cast<QQuickWindow*>(parent))
{
    if(m_item != nullptr) {
        connect(m_item, &QQuickItem::windowChanged,
                this, &cwWindowOverlayAttachedType::updateOverlay);
    } else if(m_window == nullptr) {
        //Only items and windows belong to a window. Anything else - a QtObject,
        //a Popup - would resolve to no overlay forever, which is invisible at
        //the call site, so say so once here instead.
        qWarning() << "WindowOverlay attached to" << parent
                   << "which is neither an Item nor a Window, so it is in no"
                   << "window and will never find an overlay";
    }

    //The overlay usually registers before anything looks for it, but nothing
    //guarantees the order, and a binding that resolved null would stay null
    connect(cwWindowOverlayRegistry::instance(), &cwWindowOverlayRegistry::overlaysChanged,
            this, &cwWindowOverlayAttachedType::updateOverlay);

    updateOverlay();
}

QQuickWindow* cwWindowOverlayAttachedType::attacheeWindow() const
{
    if(m_item != nullptr) {
        return m_item->window();
    }
    return m_window;
}

void cwWindowOverlayAttachedType::updateOverlay()
{
    auto overlay = cwWindowOverlayRegistry::instance()->overlay(attacheeWindow());

    if(m_overlay == overlay) {
        return;
    }

    m_overlay = overlay;
    emit overlayChanged();
}
