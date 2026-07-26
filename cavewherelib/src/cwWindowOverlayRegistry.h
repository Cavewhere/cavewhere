/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWWINDOWOVERLAYREGISTRY_H
#define CWWINDOWOVERLAYREGISTRY_H

//Qt includes
#include <QHash>
#include <QObject>
#include <QPointer>

//Our includes
#include "CaveWhereLibExport.h"
class cwWindowOverlay;
class QQuickWindow;

/**
 * Which overlay belongs to which window.
 *
 * The lookup is by window because an overlay is a sibling of the content it
 * floats above, not an ancestor of it, so there is no parent chain to walk.
 */
class CAVEWHERE_LIB_EXPORT cwWindowOverlayRegistry : public QObject
{
    Q_OBJECT

public:
    static cwWindowOverlayRegistry* instance();

    void addOverlay(QQuickWindow* window, cwWindowOverlay* overlay);
    void removeOverlay(QQuickWindow* window, cwWindowOverlay* overlay);
    cwWindowOverlay* overlay(QQuickWindow* window) const;

signals:
    //! A window gained or lost its overlay. Carries no window on purpose: the
    //! listeners are attached objects that may have changed window themselves.
    void overlaysChanged();

private:
    cwWindowOverlayRegistry() = default;

    QHash<QQuickWindow*, QPointer<cwWindowOverlay>> m_overlays;
};

#endif // CWWINDOWOVERLAYREGISTRY_H
