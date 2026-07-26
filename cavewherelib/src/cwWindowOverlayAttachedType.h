/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWWINDOWOVERLAYATTACHEDTYPE_H
#define CWWINDOWOVERLAYATTACHEDTYPE_H

//Qt includes
#include <QObject>
#include <QPointer>
#include <QQmlEngine>

//Our includes
#include "CaveWhereLibExport.h"
//Complete, not forward-declared: the overlay Q_PROPERTY's metatype and the
//QPointer accessor below both need the full type. cwWindowOverlay.h only
//forward-declares this class in return, so the include stays one-directional.
#include "cwWindowOverlay.h"
class QQuickItem;
class QQuickWindow;

/**
 * The overlay of whichever window the object this is attached to lives in.
 *
 * Attach it to any item to reach its window's overlay without knowing where
 * either sits in the tree:
 *
 *     WindowOverlay.overlay
 *
 * The value follows the item between windows rather than being resolved once,
 * so an item torn off into a new window keeps working (issue #494).
 */
class CAVEWHERE_LIB_EXPORT cwWindowOverlayAttachedType : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(cwWindowOverlay* overlay READ overlay NOTIFY overlayChanged)

public:
    explicit cwWindowOverlayAttachedType(QObject* parent);

    cwWindowOverlay* overlay() const { return m_overlay.data(); }

signals:
    void overlayChanged();

private:
    void updateOverlay();
    QQuickWindow* attacheeWindow() const;

    //At most one of these is set, depending on what this attached to. Both stay
    //null when it attached to something that is in no window at all, which
    //resolves to no overlay - see the constructor.
    QPointer<QQuickItem> m_item;
    QPointer<QQuickWindow> m_window;

    QPointer<cwWindowOverlay> m_overlay;
};

#endif // CWWINDOWOVERLAYATTACHEDTYPE_H
