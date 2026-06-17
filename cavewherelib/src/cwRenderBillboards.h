#ifndef CWRENDERBILLBOARDS_H
#define CWRENDERBILLBOARDS_H

// Renders a set of live QQuickItems as depth-occluded billboards inside the
// cwRhiScene 3D pass (issues #535 leads / #536 station labels). Each billboard
// is a QML item placed at a world position; it depth-tests against cave
// geometry and the point cloud, so a label behind a wall is hidden. Built on
// the Phase-0 encapsulation classes (cwQuickItemSubscene + cwItem2DRenderer),
// so no private Qt API leaks through this surface.
//
// Threading: the GUI object owns one cwQuickItemSubscene per content item and
// references it into the scene graph (GUI thread only). The render-thread
// backend snapshots plain data via buildRenderSlots() during synchronize()
// (when the GUI thread is blocked) and never dereferences a QQuickItem or
// QQuickWindow afterwards.

//Our includes
#include "cwRenderObject.h"

//Qt includes
#include <QVector3D>
#include <QSizeF>
#include <QList>
#include <QVector>
#include <QPointer>

//Std includes
#include <memory>
#include <unordered_map>

class QQuickItem;
class QQuickWindow;
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
        quintptr rootNodeHandle = 0;
        QSizeF contentSize;
        QVector3D worldPosition;
        SizeMode sizeMode = SizeMode::ScreenConstant;
        float depthBias = 0.0f;
    };

    explicit cwRenderBillboards(QObject* parent = nullptr);
    ~cwRenderBillboards();

    // The window whose scene graph hosts the content items. Set this before
    // setBillboards(); content items are expected to live in this window.
    void setWindow(QQuickWindow* window);
    QQuickWindow* window() const;

    void setBillboards(const QList<Billboard>& billboards);
    const QList<Billboard>& billboards() const { return m_billboards; }

    // Render-thread handoff: packs the current billboards into plain data. Reads
    // QQuickItem state, so it MUST be called only while the GUI thread is
    // blocked (from the backend's synchronize()).
    QVector<RenderSlot> buildRenderSlots() const;

protected:
    cwRHIObject* createRHIObject() override;

private:
    void rebuildSubscenes();

    QPointer<QQuickWindow> m_window;
    QList<Billboard> m_billboards;
    std::unordered_map<QQuickItem*, std::unique_ptr<cwQuickItemSubscene>> m_subscenes;
};

#endif // CWRENDERBILLBOARDS_H
