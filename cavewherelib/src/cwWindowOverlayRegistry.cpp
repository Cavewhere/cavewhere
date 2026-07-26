/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwWindowOverlayRegistry.h"
#include "cwWindowOverlay.h"

cwWindowOverlayRegistry* cwWindowOverlayRegistry::instance()
{
    static cwWindowOverlayRegistry registry;
    return &registry;
}

void cwWindowOverlayRegistry::addOverlay(QQuickWindow* window, cwWindowOverlay* overlay)
{
    if(window == nullptr || overlay == nullptr) {
        return;
    }

    auto existing = m_overlays.value(window);
    if(existing != nullptr && existing != overlay) {
        qWarning() << "Window already has an overlay, replacing it. A second one"
                   << "would leave everything that already resolved the first"
                   << "pointing at the wrong item:" << window;
    }

    m_overlays.insert(window, overlay);
    emit overlaysChanged();
}

void cwWindowOverlayRegistry::removeOverlay(QQuickWindow* window, cwWindowOverlay* overlay)
{
    //Only the overlay that registered may deregister, or an overlay leaving a
    //window it was replaced in would take the replacement with it
    if(window == nullptr || m_overlays.value(window) != overlay) {
        return;
    }

    m_overlays.remove(window);
    emit overlaysChanged();
}

cwWindowOverlay* cwWindowOverlayRegistry::overlay(QQuickWindow* window) const
{
    return window != nullptr ? m_overlays.value(window).data() : nullptr;
}
