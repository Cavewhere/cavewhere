#ifndef CWRENDERBILLBOARDS_H
#define CWRENDERBILLBOARDS_H

// Renders a set of live QQuickItems as depth-occluded billboards inside the
// cwRhiScene 3D pass (issues #535 leads / #536 station labels). Each billboard
// is a QML item placed at a world position; it depth-tests against cave
// geometry and the point cloud, so a label behind a wall is hidden. Built on
// the Phase-0 encapsulation classes (cwQuickItemSubscene + cwItem2DRenderer),
// so no private Qt API leaks through this surface.
//
// Identity: billboards are added/updated/removed individually by a stable
// cwBillboardId (returned by addBillboard). The render-side renderer cache is
// keyed by that id so a billboard's cached scene-graph render list survives
// other billboards being inserted or removed — positional keying would re-bind
// renderers to the wrong content past a removal (the cwRenderObjectId hazard,
// issue #512, which is exactly what happens when a pooled content item is
// recycled across stations).
//
// Each billboard owns one cwQuickItemSubscene for its content item; there is no
// content sharing (one billboard, one content item). refFromEffectItem is itself
// reference-counted inside Qt, so even if two billboards referenced the same
// item it would stay valid until both released it.
//
// Threading: the GUI object owns the subscenes and references them into the
// scene graph (GUI thread only). The render-thread backend snapshots plain data
// via buildRenderSlots() during synchronize() (when the GUI thread is blocked)
// and never dereferences a QQuickItem or QQuickWindow afterwards.

//Our includes
#include "cwRenderObject.h"
#include "cwBillboardId.h"

//Qt includes
#include <QVector3D>
#include <QSizeF>
#include <QVector>
#include <QPointer>

//Std includes
#include <memory>
#include <unordered_map>

class QQuickItem;
class QQuickWindow;
class QMatrix4x4;
class cwQuickItemSubscene;

class CAVEWHERE_LIB_EXPORT cwRenderBillboards : public cwRenderObject
{
    Q_OBJECT
public:
    enum class SizeMode {
        WorldSized,      // scales with the 3D scene (a fixed world size)
        ScreenConstant,  // fixed pixel size on screen regardless of distance
    };
    Q_ENUM(SizeMode)

    struct Billboard {
        QQuickItem* content = nullptr;
        QVector3D worldPosition;
        SizeMode sizeMode = SizeMode::ScreenConstant;
        float depthBias = 0.0f;   // world units to offset toward the eye
    };

    // Per-billboard data snapshotted on the GUI thread (during synchronize, when
    // the GUI thread is blocked) and consumed on the render thread. Holds only
    // plain values plus an opaque scene-graph node handle — never a QQuickItem
    // pointer to dereference on the render thread (the D-002 invariant).
    struct RenderSlot {
        cwBillboardId id = cwBillboardId{};
        quintptr rootNodeHandle = 0;
        QSizeF contentSize;
        QVector3D worldPosition;
        SizeMode sizeMode = SizeMode::ScreenConstant;
        float depthBias = 0.0f;
    };

    explicit cwRenderBillboards(QObject* parent = nullptr);
    ~cwRenderBillboards();

    // The window whose scene graph hosts the content items. Set this before
    // adding billboards; content items are expected to live in this window.
    void setWindow(QQuickWindow* window);
    QQuickWindow* window() const;

    // Granular, stable-id API. addBillboard returns the id the caller keeps for
    // later update/remove. updateBillboard is a no-op when nothing changed, so
    // callers may re-push an unchanged billboard cheaply each frame.
    cwBillboardId addBillboard(const Billboard& billboard);
    void updateBillboard(cwBillboardId id, const Billboard& billboard);
    void removeBillboard(cwBillboardId id);
    bool hasBillboard(cwBillboardId id) const;
    int billboardCount() const;

    // Render-thread handoff: packs the current billboards into plain data. Reads
    // QQuickItem state, so it MUST be called only while the GUI thread is
    // blocked (from the backend's synchronize()).
    QVector<RenderSlot> buildRenderSlots() const;

protected:
    cwRHIObject* createRHIObject() override;

private:
    // One billboard: its parameters plus the subscene that references its content
    // item into the scene graph. The subscene is null until a window is set.
    struct Entry {
        Billboard billboard;
        std::unique_ptr<cwQuickItemSubscene> subscene;
    };

    std::unique_ptr<cwQuickItemSubscene> makeSubscene(QQuickItem* content) const;

    QPointer<QQuickWindow> m_window;
    uint32_t m_nextId = 1;
    std::unordered_map<cwBillboardId, Entry> m_billboards;
};

// Sorts billboard render slots back-to-front (farthest from the eye first) by the
// NDC depth of each slot's world position through @a viewProjection. Every quad
// is a parallel, camera-facing plane, so center-depth order equals per-pixel
// order everywhere on the quad — drawing in this order keeps an overlapping
// billboard's transparent quad corners (which still write depth in RenderMode3D)
// from rejecting the glyph pixels of a farther billboard behind it (#538). Stable,
// so equal-depth billboards keep their insertion order. Pure (no QRhi), so it's
// unit-tested directly.
void CAVEWHERE_LIB_EXPORT cwSortBillboardSlotsBackToFront(
    QVector<cwRenderBillboards::RenderSlot>& renderSlots,
    const QMatrix4x4& viewProjection);

#endif // CWRENDERBILLBOARDS_H
