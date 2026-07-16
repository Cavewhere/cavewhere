#ifndef CWQUICKITEMSUBSCENE_H
#define CWQUICKITEMSUBSCENE_H

// Makes a QQuickItem renderable off the normal 2D overlay tree, so its
// scene-graph subtree can be drawn inline in a 3D pass by cwItem2DRenderer.
//
// Construction references the item into the window's scene graph
// (refWindow + refFromEffectItem) which builds — and hides — its node subtree,
// exactly like ShaderEffectSource or QtQuick3D's Item2D content. Destruction
// reverses it. All ref/deref must happen on the GUI thread.
//
// This is one of only two files in CaveWhere that touch Qt private API (the
// other is cwItem2DRenderer). The QSGRootNode never leaks into this header — it
// is handed out as an opaque quintptr handle.

//Qt includes
#include <QPointer>

class QQuickItem;
class QQuickWindow;

class cwQuickItemSubscene
{
public:
    cwQuickItemSubscene(QQuickItem* content, QQuickWindow* window);
    ~cwQuickItemSubscene();

    cwQuickItemSubscene(const cwQuickItemSubscene&) = delete;
    cwQuickItemSubscene& operator=(const cwQuickItemSubscene&) = delete;

    // True once the scene graph has materialized the content's node subtree
    // (it may be null for the first frame or two after construction).
    bool nodeReady() const;

    // Opaque QSGRootNode pointer for cwItem2DRenderer::Frame::rootNodeHandle.
    // Returns 0 until nodeReady() is true.
    quintptr rootNodeHandle() const;

    // Forces the dirty content item to sync its node subtree. Call during the
    // window's synchronize phase (when the GUI thread is blocked) if the node
    // isn't ready yet.
    void forceSync();

    QQuickItem* content() const { return m_content; }
    QQuickWindow* window() const { return m_window; }

private:
    QPointer<QQuickItem> m_content;
    QPointer<QQuickWindow> m_window;
    bool m_referenced = false;
};

#endif // CWQUICKITEMSUBSCENE_H
