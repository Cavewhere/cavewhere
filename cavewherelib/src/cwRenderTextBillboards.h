/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRENDERTEXTBILLBOARDS_H
#define CWRENDERTEXTBILLBOARDS_H

// Renders a set of text strings (station labels) as depth-occluded,
// camera-facing billboards inside the cwRhiScene 3D pass, all of them through
// one scene-graph renderer per frame. Lead markers draw through
// cwRenderSpriteBillboards (one instanced draw of one prototype) and unique
// QQuickItem billboards through cwRenderBillboards; this layer is for bulk text
// that needs no item, no input and no per-label renderer.
//
// Identity mirrors cwRenderBillboards: billboards are added, updated and removed
// individually by a stable cwBillboardId, and the render side keys its text nodes
// by that id.
//
// Threading: text layout (QTextLayout shaping) runs on the GUI thread, once per
// distinct string, and is cached. The render-thread backend snapshots plain data
// plus shared pointers to the finished, immutable layouts via buildRenderSlots()
// during synchronize() (GUI thread blocked), touches a layout only inside
// synchronize, and releases its references only there or at teardown.

//Our includes
#include "cwRenderObject.h"
#include "cwBillboardId.h"
#include "CaveWhereLibExport.h"

//Qt includes
#include <QColor>
#include <QHash>
#include <QFont>
#include <QPointer>
#include <QSizeF>
#include <QString>
#include <QVector>
#include <QVector3D>

//Std includes
#include <memory>
#include <unordered_map>

class QQuickWindow;
class QTextLayout;

class CAVEWHERE_LIB_EXPORT cwRenderTextBillboards : public cwRenderObject
{
    Q_OBJECT
public:
    struct TextBillboard {
        QString text;
        QVector3D worldPosition;
        float depthBias = 0.0f;   // world units toward the viewer (cwBillboardSightLine)

        bool operator==(const TextBillboard&) const = default;
    };

    // Visual style shared by every label in the layer. Set from QML through
    // Label3dView properties bound to Theme tokens.
    struct Style {
        QFont font;
        QColor color;
        QColor outlineColor;   // QSGTextNode::Outline style color

        bool operator==(const Style&) const = default;
    };

    // Render-thread handoff: plain data plus a shared pointer to the immutable,
    // already laid-out QTextLayout of each label.
    struct RenderSlot {
        cwBillboardId id = cwBillboardId{};
        std::shared_ptr<const QTextLayout> layout;   // built on the GUI thread
        QSizeF contentSize;
        QVector3D worldPosition;
        float depthBias = 0.0f;
        // Render-thread scratch for cw::billboard::sortBackToFront; buildRenderSlots
        // leaves it 0.
        float sortDepth = 0.0f;
    };

    explicit cwRenderTextBillboards(QObject* parent = nullptr);
    ~cwRenderTextBillboards();

    // The window whose scene graph creates the text nodes
    // (QQuickWindow::createTextNode).
    void setWindow(QQuickWindow* window);
    QQuickWindow* window() const;

    void setStyle(const Style& style);
    Style style() const;

    // Stable-id API, mirroring cwRenderBillboards. updateBillboard is a no-op when
    // nothing changed, so callers may re-push an unchanged billboard each frame.
    cwBillboardId addBillboard(const TextBillboard& billboard);
    void updateBillboard(cwBillboardId id, const TextBillboard& billboard);
    void removeBillboard(cwBillboardId id);
    bool hasBillboard(cwBillboardId id) const;
    int billboardCount() const;

    // Laid-out size of @a text in logical pixels with the current style; the label
    // view uses it as the exact declutter rect for a placed label. Cached per
    // distinct string.
    QSizeF textSize(const QString& text);

    // Packs the current billboards into plain data for the render thread. Called
    // from the backend's synchronize() while the GUI thread is blocked.
    QVector<RenderSlot> buildRenderSlots() const;

protected:
    cwRHIObject* createRHIObject() override;

private:
    // One distinct string laid out with the current style, shared by every label
    // showing it. GUI thread only; the render thread sees just the layout.
    struct CachedLayout {
        std::shared_ptr<const QTextLayout> layout;
        QSizeF size;
    };

    struct Entry {
        TextBillboard billboard;
        std::shared_ptr<CachedLayout> layout;
    };

    std::shared_ptr<CachedLayout> acquireLayout(const QString& text);
    void releaseLayout(const QString& text);

    QPointer<QQuickWindow> m_window;
    Style m_style;
    uint32_t m_nextId = 1;
    std::unordered_map<cwBillboardId, Entry> m_billboards;
    QHash<QString, std::shared_ptr<CachedLayout>> m_layouts;
};

#endif // CWRENDERTEXTBILLBOARDS_H
