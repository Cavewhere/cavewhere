/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRENDERSPRITEBILLBOARDS_H
#define CWRENDERSPRITEBILLBOARDS_H

// Draws every billboard in the layer with one instanced RHI draw that samples
// one shared texture: the look of a single QML prototype item (issue 714). Lead
// markers are the producer this exists for; every lead shows the same question
// mark, so one texture and one draw replace one scene-graph renderer per lead.
//
// The prototype is referenced into the window's scene graph (cwQuickItemSubscene,
// hidden from the 2D tree) and rendered once into a texture by a QSGLayer
// (cwQuickItemLayerTexture). Each billboard is one instance: a world position,
// a depth bias toward the viewer, an opacity and a visible flag. The vertex
// shader expands a fixed 4-vertex TriangleStrip per instance into a
// camera-facing quad of the prototype's size, in logical pixels, using the
// global camera UBO slot, so the same draw renders the live frame and offscreen
// export tiles with their own cameras.
//
// Depth follows the marker's silhouette: the fragment shader discards fragments
// below an alpha cutoff, so they write no depth and whatever draws after this
// layer (station labels) blends correctly through the marker's transparent
// pixels and is hidden only by its ink.
//
// Identity mirrors cwRenderBillboards: billboards are added, updated and removed
// by a stable cwBillboardId, and instances upload in id order. A hidden billboard
// stays in the store and is left out of the instance buffer, so a keyword filter
// toggle costs one update rather than a remove and a re-add.
//
// Threading: the GUI object owns the store and the prototype's subscene. The
// render-thread backend snapshots plain data through buildInstances(),
// spriteSize() and spriteRootNodeHandle() during synchronize(), while the GUI
// thread is blocked, and dereferences no QQuickItem afterwards.

//Our includes
#include "cwRenderObject.h"
#include "cwBillboardId.h"
#include "cwBillboardGeometry.h"
#include "CaveWhereLibExport.h"

//Qt includes
#include <QPointer>
#include <QSizeF>
#include <QVector>
#include <QVector3D>

//Std includes
#include <map>
#include <memory>

class QQuickItem;
class QQuickWindow;
class cwQuickItemSubscene;

class CAVEWHERE_LIB_EXPORT cwRenderSpriteBillboards : public cwRenderObject
{
    Q_OBJECT
public:
    struct SpriteBillboard {
        QVector3D worldPosition;
        float depthBias = 0.0f;   // world units toward the viewer (cwBillboardSightLine)
        float opacity = 1.0f;
        bool visible = true;

        bool operator==(const SpriteBillboard&) const = default;
    };

    // GPU layout of one instance; uploaded verbatim as the per-instance vertex
    // buffer (Float3 at 0, Float at 12, Float at 16).
    struct Instance {
        QVector3D worldPosition;
        float depthBias = 0.0f;
        float opacity = 1.0f;

        bool operator==(const Instance&) const = default;
    };

    explicit cwRenderSpriteBillboards(QObject* parent = nullptr);
    ~cwRenderSpriteBillboards() override;

    // The window whose scene graph renders the prototype; all billboards share it.
    void setWindow(QQuickWindow* window);
    QQuickWindow* window() const;

    // The one QML item every billboard draws. Referenced into the window's scene
    // graph (hidden from the 2D tree) and rendered to the shared texture. Its
    // width and height in logical pixels are the sprite size.
    void setSprite(QQuickItem* prototype);
    QQuickItem* sprite() const;
    QSizeF spriteSize() const;

    // ScreenConstant by default: the sprite keeps its pixel size at any distance.
    void setSizeMode(cwBillboardSizeMode sizeMode);
    cwBillboardSizeMode sizeMode() const;

    // Stable-id API, mirroring cwRenderBillboards. updateBillboard is a no-op when
    // nothing changed, so callers may re-push an unchanged billboard each frame.
    cwBillboardId addBillboard(const SpriteBillboard& billboard);
    void updateBillboard(cwBillboardId id, const SpriteBillboard& billboard);
    void removeBillboard(cwBillboardId id);
    bool hasBillboard(cwBillboardId id) const;
    int billboardCount() const;

    // The visible billboards as the render thread uploads them, in id order.
    // Called from the backend's synchronize() while the GUI thread is blocked.
    QVector<Instance> buildInstances() const;

    // The prototype's scene-graph root; 0 without a sprite and a window. Builds
    // the prototype's nodes when the window's sync has yet to reach them, so it
    // is read from synchronize() only, while the GUI thread is blocked.
    quintptr spriteRootNodeHandle() const;

protected:
    cwRHIObject* createRHIObject() override;

private:
    void rebuildSubscene();

    QPointer<QQuickWindow> m_window;
    QPointer<QQuickItem> m_sprite;
    std::unique_ptr<cwQuickItemSubscene> m_spriteSubscene;
    cwBillboardSizeMode m_sizeMode = cwBillboardSizeMode::ScreenConstant;
    // Ordered so the instance order is stable across frames.
    std::map<cwBillboardId, SpriteBillboard> m_billboards;
    uint32_t m_nextId = 1;
};

#endif // CWRENDERSPRITEBILLBOARDS_H
