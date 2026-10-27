/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWWINDOWOVERLAY_H
#define CWWINDOWOVERLAY_H

//Qt includes
#include <QPointer>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>

//Our includes
#include "CaveWhereLibExport.h"
class cwWindowOverlayAttachedType;

/**
 * The item that floats above a window's content: the shared text editor lives
 * in it, and popups reparent into it so nothing clips them.
 *
 * One per window. It registers itself for whichever window it is in, so
 * anything in that window can find it with the attached property no matter
 * where it sits in the tree - see cwWindowOverlayAttachedType.
 *
 * A QML singleton cannot do this job. There is one QQmlEngine for the whole
 * application, so a singleton is shared by every window, and an Item has one
 * parent: the second window to claim it takes it away from the first, and the
 * first window's fields then open an editor in the wrong window (issue #494).
 */
class CAVEWHERE_LIB_EXPORT cwWindowOverlay : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(WindowOverlay)
    QML_ATTACHED(cwWindowOverlayAttachedType)

public:
    explicit cwWindowOverlay(QQuickItem* parentItem = nullptr);
    ~cwWindowOverlay() override;

    /**
     * Attaches to object. Only the Qt framework should call this.
     */
    static cwWindowOverlayAttachedType* qmlAttachedProperties(QObject* object);

protected:
    void itemChange(ItemChange change, const ItemChangeData& data) override;

private:
    QPointer<QQuickWindow> m_registeredWindow;
};

#endif // CWWINDOWOVERLAY_H
