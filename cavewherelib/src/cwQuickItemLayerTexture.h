/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWQUICKITEMLAYERTEXTURE_H
#define CWQUICKITEMLAYERTEXTURE_H

// Renders a QML item's scene-graph subtree into a QRhiTexture, exactly as
// `layer.enabled` does for any item, so CaveWhere's own RHI pipelines can
// sample the item's look. Wraps a QSGLayer from the window's scene-graph
// context; the layer marks itself dirty from the subtree's changes and renders
// again only then, so a static item costs one render.
//
// Render thread only: construct, update and destroy it there, and destroy it
// before the window's scene graph invalidates (QQuickWindow::sceneGraphInvalidated,
// connected directly). The root node arrives as the opaque handle
// cwQuickItemSubscene hands out.
//
// A subtree change dirties the layer, and the layer asks the GUI-thread render
// object for a frame (a queued call to cwRenderObject::update, as
// QQuickShaderEffectSource does for its item), so the next gather re-renders
// the texture instead of waiting for an unrelated update.
//
// This is one of only three files in CaveWhere that touch Qt private API (the
// others are cwQuickItemSubscene and cwItem2DRenderer).

//Qt includes
#include <QSizeF>
#include <QtGlobal>

//Std includes
#include <memory>

class QQuickWindow;
class QRhiTexture;
class cwRenderObject;

class cwQuickItemLayerTexture
{
public:
    // updateReceiver is the GUI-thread render object that draws this texture; it
    // is asked to update whenever the layer turns dirty.
    cwQuickItemLayerTexture(QQuickWindow* window, cwRenderObject* updateReceiver);
    ~cwQuickItemLayerTexture();

    cwQuickItemLayerTexture(const cwQuickItemLayerTexture&) = delete;
    cwQuickItemLayerTexture& operator=(const cwQuickItemLayerTexture&) = delete;

    // True when the window's scene graph handed out a layer; false leaves every
    // other call a no-op and rhiTexture() null.
    bool isValid() const;

    // The subtree to render, as cwQuickItemSubscene::rootNodeHandle() reports it;
    // 0 releases the texture.
    void setRootNodeHandle(quintptr rootNodeHandle);

    // The item's size in logical pixels and the ratio the texture renders at.
    void setContentSize(const QSizeF& logicalSize, float devicePixelRatio);

    // Renders the subtree when it changed since the last call. Records its own
    // render pass on the current frame's command buffer, so call it outside any
    // open pass (from gather). True when the layer rendered this call.
    bool updateTexture();

    // The texture of the last updateTexture(). Null before the first one. The
    // pointer changes when the layer resizes, so callers compare it each frame
    // and rebuild their shader resource bindings on a change.
    QRhiTexture* rhiTexture() const;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

#endif // CWQUICKITEMLAYERTEXTURE_H
