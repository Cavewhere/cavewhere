/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRENDERSPRITEBILLBOARDS_H
#define CWRENDERSPRITEBILLBOARDS_H

// Draws every billboard in the layer with one instanced RHI draw that samples
// one atlas texture holding the look of a few QML prototype items, its slots
// (issue 714). Lead markers are the producer this exists for; one atlas and one
// draw replace one scene-graph renderer per lead.
//
// Scope: the layer is for many instances of a few fixed-size icons, each with
// its own position, depth bias, opacity, visibility and slot. Every slot shares
// one size, spriteSize() (slot 0's). Content that needs a per-instance size,
// per-instance text or live per-instance content belongs on cwRenderBillboards,
// which renders each billboard's own item subtree.
//
// Each prototype is referenced into the window's scene graph (cwQuickItemSubscene,
// hidden from the 2D tree) and rendered into its own texture by a QSGLayer
// (cwQuickItemLayerTexture). The backend copies each slot's texture into its
// cell of the atlas, one row of spriteSize cells in device pixels, whenever the
// slot re-renders. Each billboard is one instance: a world position, a depth
// bias toward the viewer, an opacity, a visible flag and a slot index. The
// vertex shader expands a fixed 4-vertex TriangleStrip per instance into a
// camera-facing quad of spriteSize, in logical pixels, using the global camera
// UBO slot, so the same draw renders the live frame and offscreen export tiles
// with their own cameras.
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
// Threading: the GUI object owns the store and every slot's subscene. The
// render-thread backend snapshots plain data through buildInstances(),
// spriteCount(), spriteSize() and spriteRootNodeHandle(slot) during
// synchronize(), while the GUI thread is blocked, and dereferences no
// QQuickItem afterwards.

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
#include <vector>

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
        int spriteIndex = 0;      // slot in the atlas, see setSprites

        bool operator==(const SpriteBillboard&) const = default;
    };

    // GPU layout of one instance; uploaded verbatim as the per-instance vertex
    // buffer (Float3 at 0, Float at 12, Float at 16, UInt at 20).
    struct Instance {
        QVector3D worldPosition;
        float depthBias = 0.0f;
        float opacity = 1.0f;
        uint32_t spriteIndex = 0;

        bool operator==(const Instance&) const = default;
    };

    explicit cwRenderSpriteBillboards(QObject* parent = nullptr);
    ~cwRenderSpriteBillboards() override;

    // The window whose scene graph renders the prototype; all billboards share it.
    void setWindow(QQuickWindow* window);
    QQuickWindow* window() const;

    // Atlas slots. Slot order is the atlas order and the spriteIndex instances
    // use. Each prototype is referenced into the window's scene graph (hidden
    // from the 2D tree) and rendered into its cell. Every slot shares
    // spriteSize(), the size of slot 0 in logical pixels; a slot of a different
    // size is drawn at that shared size and reported once with qWarning. Null
    // entries are skipped and the list is capped at kMaxSprites slots, both
    // with a warning. A destroyed prototype keeps its slot, so the other
    // indices stay.
    void setSprites(const QList<QQuickItem*>& prototypes);
    QList<QQuickItem*> sprites() const;
    int spriteCount() const;
    QSizeF spriteSize() const;

    // Single-slot form: setSprite(item) is setSprites({item}), and null clears.
    void setSprite(QQuickItem* prototype);
    QQuickItem* sprite() const;   // slot 0 or null

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

    // The visible billboards as the render thread uploads them, in id order. A
    // billboard naming a slot outside [0, spriteCount()) is left out and warned
    // about once per store; with no slots declared every visible one is kept.
    // Called from the backend's synchronize() while the GUI thread is blocked.
    QVector<Instance> buildInstances() const;

    // The scene-graph root of @a slot's prototype; 0 out of range, for a
    // destroyed prototype, or without a window. Builds the prototype's nodes
    // when the window's sync has yet to reach them, so it is read from
    // synchronize() only, while the GUI thread is blocked.
    quintptr spriteRootNodeHandle(int slot) const;

protected:
    cwRHIObject* createRHIObject() override;

private:
    struct Slot {
        QPointer<QQuickItem> prototype;
        std::unique_ptr<cwQuickItemSubscene> subscene;
    };

    void rebuildSubscenes();
    void prototypeResized();
    void warnOnSizeMismatch();

    QPointer<QQuickWindow> m_window;
    std::vector<Slot> m_slots;
    bool m_warnedSizeMismatch = false;
    mutable bool m_warnedMissingSlot = false;
    cwBillboardSizeMode m_sizeMode = cwBillboardSizeMode::ScreenConstant;
    // Ordered so the instance order is stable across frames.
    std::map<cwBillboardId, SpriteBillboard> m_billboards;
    uint32_t m_nextId = 1;
};

#endif // CWRENDERSPRITEBILLBOARDS_H
