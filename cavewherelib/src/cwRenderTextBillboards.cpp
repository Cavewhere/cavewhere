/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwRenderTextBillboards.h"
#include "cwBillboardGeometry.h"
#include "cwItem2DRenderer.h"
#include "cwProfileLog.h"
#include "cwRHIObject.h"
#include "cwRhiItemRenderer.h"

//Qt includes
#include <QElapsedTimer>
#include <QQuickWindow>
#include <QSGNode>
#include <QSGTextNode>
#include <QTextLayout>
#include <rhi/qrhi.h>

//Std includes
#include <cfloat>
#include <cmath>
#include <utility>

namespace {

// A single unwrapped line: QTextLine clamps this to its own maximum.
constexpr qreal kUnboundedLineWidth = FLT_MAX;

std::shared_ptr<const QTextLayout> makeLayout(const QString& text, const QFont& font, QSizeF* size)
{
    auto layout = std::make_shared<QTextLayout>(text, font);
    layout->setCacheEnabled(true);
    layout->beginLayout();
    QTextLine line = layout->createLine();
    QSizeF lineSize;
    if (line.isValid()) {
        line.setLineWidth(kUnboundedLineWidth);
        line.setPosition(QPointF(0.0, 0.0));
        lineSize = QSizeF(std::ceil(line.naturalTextWidth()), std::ceil(line.height()));
    }
    layout->endLayout();
    *size = lineSize;
    return layout;
}

// Render-thread backend: one scene-graph renderer for every label in the layer.
// Each label is two QSGTextNodes (outline and fill) under one QSGRootNode, drawn
// inline in the Overlay pass by a single cwItem2DRenderer so the labels
// depth-test against the cave like the item layer does.
class cwRHITextBillboards : public cwRHIObject
{
public:
    ~cwRHITextBillboards() override
    {
        teardownNodes();
    }

    void initialize(const ResourceUpdateData&) override {}
    void updateResources(const ResourceUpdateData&) override {}

    // One MVP per gather, like the item layer: an atlas-batched offscreen render
    // would draw every tile with the last tile's pose.
    bool precludesAtlasBatching() const override { return !m_slots.isEmpty(); }

    void synchronize(const SynchronizeData& data) override
    {
        auto* renderObject = static_cast<cwRenderTextBillboards*>(data.object);

        QQuickWindow* window = renderObject->window();
        if (window != m_window) {
            bindWindow(window);
        }

        if (m_window.isNull() || !m_window->isSceneGraphInitialized()) {
            teardownNodes();
            return;
        }

        // Snapshot while the GUI thread is blocked. Layouts are touched (glyph runs
        // copied into nodes) and released only here or in teardownNodes().
        m_slots = renderObject->buildRenderSlots();

        const cwRenderTextBillboards::Style style = renderObject->style();
        const bool styleChanged = style != m_style;
        m_style = style;

        ensureRoot();

        // Move survivors into a fresh map; labels whose id is gone are deleted with
        // the old one, which also unlinks their nodes from the subtrees.
        std::unordered_map<cwBillboardId, LabelNodes> labels;
        labels.reserve(m_slots.size());
        bool treeChanged = false;
        for (const cwRenderTextBillboards::RenderSlot& slot : std::as_const(m_slots)) {
            auto existing = m_labels.find(slot.id);
            if (existing != m_labels.end()) {
                LabelNodes& nodes = labels.emplace(slot.id, std::move(existing->second)).first->second;
                m_labels.erase(existing);
                if (styleChanged || nodes.layout != slot.layout) {
                    setGlyphs(nodes, slot.layout);
                }
            } else {
                LabelNodes& nodes = labels.emplace(slot.id, createNodes()).first->second;
                setGlyphs(nodes, slot.layout);
                treeChanged = true;
            }
        }
        treeChanged = treeChanged || !m_labels.empty();
        m_labels = std::move(labels);

        if (treeChanged) {
            // New nodes were appended at the end and removed ones unlinked, so the
            // child order no longer matches the last sort.
            m_childOrder.clear();
        }
    }

    bool gather(const GatherContext& context, QVector<PipelineBatch>& batches) override
    {
        if (context.renderPass != RenderPass::Overlay || m_slots.isEmpty() || !m_root) {
            return false;
        }

        const bool profile = context.liveFrame && lcProfileRender().isDebugEnabled();
        QElapsedTimer timer;
        if (profile) {
            timer.start();
        }

        const RenderData& renderData = *context.renderData;
        cwRhiItemRenderer* renderer = renderData.renderer;
        if (!renderer || !renderer->rhi()) {
            return false;
        }

        // Camera, target and DPR come from the render job rather than the
        // renderer's live-frame accessors, so an offscreen render projects labels with its
        // own camera (see cwRHIBillboards::gather).
        QRhiRenderTarget* target = renderData.renderTarget;
        if (!target) {
            return false;
        }

        const QSize pixelSize = target->pixelSize();
        const float devicePixelRatio = renderData.devicePixelRatio;
        const cw::billboard::FrameCamera frameCamera(renderData.viewMatrix,
                                                     renderData.projectionMatrix,
                                                     renderData.viewProjectionMatrix,
                                                     pixelSize, devicePixelRatio);
        const QMatrix4x4& viewProjection = frameCamera.viewProjection();

        // Tree order is paint order, so the subtrees follow the sort. Where two
        // labels overlap the nearer one paints last.
        cw::billboard::sortBackToFront(m_slots, viewProjection);
        orderChildren();

        for (const cwRenderTextBillboards::RenderSlot& slot : std::as_const(m_slots)) {
            auto it = m_labels.find(slot.id);
            if (it == m_labels.end()) {
                continue;
            }
            // Keep these matrices general (camera-facing, built with setColumn).
            // A translate-only matrix lets the batch renderer merge the batch,
            // which bakes x/y into the vertices and drops z, flattening depth.
            const QMatrix4x4 model = frameCamera.modelMatrix(
                slot.worldPosition, slot.contentSize, cwBillboardSizeMode::ScreenConstant,
                slot.depthBias);
            it->second.outline->setMatrix(model);
            it->second.fill->setMatrix(model);
        }

        if (!m_renderer) {
            m_renderer = std::make_unique<cwItem2DRenderer>(m_window);
        }

        // The model lives on each node, so the renderer's projection is VP alone;
        // unmerged batches compute clip = VP x node matrix, today's VP x M.
        cwItem2DRenderer::Frame frame;
        frame.mvp = viewProjection;
        frame.rt = target;
        frame.rpd = renderData.renderPassDescriptor;
        frame.cb = renderData.cb;
        frame.deviceRect = pixelSize;
        frame.devicePixelRatio = devicePixelRatio;
        frame.rootNodeHandle = reinterpret_cast<quintptr>(m_root.get());

        // Uploads happen here, before cwRhiScene opens the pass.
        if (!m_renderer->prepare(frame)) {
            return false;
        }

        const qint64 gatherUs = profile ? cw::profile::elapsedUs(timer) : 0;
        const int labelCount = m_slots.size();

        PipelineState state;
        state.pipeline = nullptr;
        state.sortKey = makeSortKey(context.objectOrder, nullptr);
        auto& batch = acquirePipelineBatch(batches, state);

        cwItem2DRenderer* renderer2d = m_renderer.get();
        Drawable drawable;
        drawable.type = Drawable::Type::Custom;
        drawable.customDraw = [renderer2d, profile, gatherUs, labelCount](const RenderData&) {
            QElapsedTimer renderTimer;
            if (profile) {
                renderTimer.start();
            }
            renderer2d->render();
            if (profile) {
                const qint64 renderUs = cw::profile::elapsedUs(renderTimer);
                cw::profile::write(lcProfileRender(),
                                   QStringLiteral("textBillboards labels=%1 overlayUs=%2 gatherUs=%3 renderUs=%4")
                                       .arg(labelCount)
                                       .arg(gatherUs + renderUs)
                                       .arg(gatherUs)
                                       .arg(renderUs));
            }
        };
        batch.drawables.append(drawable);
        return true;
    }

private:
    struct LabelNodes {
        QSGTextNode* outline = nullptr;   // owned by m_outlines
        QSGTextNode* fill = nullptr;      // owned by m_fills
        std::shared_ptr<const QTextLayout> layout;

        LabelNodes() = default;
        LabelNodes(LabelNodes&& other) noexcept
            : outline(std::exchange(other.outline, nullptr)),
              fill(std::exchange(other.fill, nullptr)),
              layout(std::move(other.layout))
        {
        }
        LabelNodes& operator=(LabelNodes&&) = delete;
        LabelNodes(const LabelNodes&) = delete;
        LabelNodes& operator=(const LabelNodes&) = delete;

        ~LabelNodes()
        {
            // Deleting a parented node unlinks it from its parent.
            delete outline;
            delete fill;
        }
    };

    void bindWindow(QQuickWindow* window)
    {
        teardownNodes();
        m_invalidationContext.reset();
        m_window = window;
        if (window != nullptr) {
            // Text nodes and the renderer belong to the window's render context
            // and have to go when it does, even if this object outlives it.
            m_invalidationContext = std::make_unique<QObject>();
            QObject::connect(window, &QQuickWindow::sceneGraphInvalidated,
                             m_invalidationContext.get(), [this]() { teardownNodes(); },
                             Qt::DirectConnection);
        }
    }

    void ensureRoot()
    {
        if (m_root) {
            return;
        }
        m_root = std::make_unique<QSGRootNode>();

        // Two subtrees, every outline first and then every fill, so the batch
        // renderer draws all labels in two batches. The trade-off: where two labels
        // overlap, the farther label's fill can paint over the nearer label's
        // outline ring; the declutter grid keeps placed labels apart, so this is
        // rare.
        //
        // The exact Label3d.qml look is one Outline node per label with the fill
        // color set. It costs about one batch per label (measured 11.1 ms vs 5.6 ms
        // of render-thread CPU per frame at 1,200 labels), because
        // QSGInternalTextNode::addGlyphs emits a second fill node whose bounding
        // box overlaps its own outline, and that overlap ends the batch group.
        m_outlines = new QSGNode();
        m_fills = new QSGNode();
        m_root->appendChildNode(m_outlines);
        m_root->appendChildNode(m_fills);
    }

    LabelNodes createNodes()
    {
        LabelNodes nodes;
        nodes.outline = m_window->createTextNode();
        nodes.fill = m_window->createTextNode();
        nodes.outline->setRenderType(QSGTextNode::QtRendering);
        nodes.fill->setRenderType(QSGTextNode::QtRendering);
        nodes.outline->setTextStyle(QSGTextNode::Outline);
        nodes.fill->setTextStyle(QSGTextNode::Normal);
        m_outlines->appendChildNode(nodes.outline);
        m_fills->appendChildNode(nodes.fill);
        return nodes;
    }

    void setGlyphs(LabelNodes& nodes, const std::shared_ptr<const QTextLayout>& layout)
    {
        // A fully transparent fill keeps the outline node from emitting its own
        // fill glyphs.
        QColor outlineFill = m_style.color;
        outlineFill.setAlpha(0);

        nodes.outline->clear();
        nodes.fill->clear();
        nodes.outline->setColor(outlineFill);
        nodes.outline->setStyleColor(m_style.outlineColor);
        nodes.fill->setColor(m_style.color);
        nodes.layout = layout;
        if (layout) {
            // addTextLayout copies the glyph runs and keeps no pointer; it reads the
            // layout's caches, which is why it only runs inside synchronize.
            auto* mutableLayout = const_cast<QTextLayout*>(layout.get());
            nodes.outline->addTextLayout(QPointF(0.0, 0.0), mutableLayout);
            nodes.fill->addTextLayout(QPointF(0.0, 0.0), mutableLayout);
        }
    }

    // Re-links the children in m_slots order when that order changed.
    void orderChildren()
    {
        const bool sameOrder = [&]() {
            if (m_childOrder.size() != m_slots.size()) {
                return false;
            }
            for (int i = 0; i < m_slots.size(); ++i) {
                if (m_childOrder.at(i) != m_slots.at(i).id) {
                    return false;
                }
            }
            return true;
        }();
        if (sameOrder) {
            return;
        }

        // Unlinks without deleting: the nodes stay owned through m_labels.
        m_outlines->removeAllChildNodes();
        m_fills->removeAllChildNodes();
        m_childOrder.clear();
        m_childOrder.reserve(m_slots.size());
        for (const cwRenderTextBillboards::RenderSlot& slot : std::as_const(m_slots)) {
            auto it = m_labels.find(slot.id);
            if (it == m_labels.end()) {
                continue;
            }
            m_outlines->appendChildNode(it->second.outline);
            m_fills->appendChildNode(it->second.fill);
            m_childOrder.append(slot.id);
        }
    }

    void teardownNodes()
    {
        // The renderer first: it holds the root node.
        m_renderer.reset();
        m_labels.clear();
        m_childOrder.clear();
        m_root.reset();
        m_outlines = nullptr;
        m_fills = nullptr;
        m_slots.clear();
        m_style = cwRenderTextBillboards::Style();
    }

    QPointer<QQuickWindow> m_window;
    std::unique_ptr<QObject> m_invalidationContext;
    cwRenderTextBillboards::Style m_style;
    QVector<cwRenderTextBillboards::RenderSlot> m_slots;

    std::unique_ptr<cwItem2DRenderer> m_renderer;
    std::unique_ptr<QSGRootNode> m_root;
    QSGNode* m_outlines = nullptr;   // owned by m_root
    QSGNode* m_fills = nullptr;      // owned by m_root
    std::unordered_map<cwBillboardId, LabelNodes> m_labels;
    QVector<cwBillboardId> m_childOrder;
};

} // namespace

cwRenderTextBillboards::cwRenderTextBillboards(QObject* parent)
    : cwRenderObject(parent)
{
}

cwRenderTextBillboards::~cwRenderTextBillboards() = default;

void cwRenderTextBillboards::setWindow(QQuickWindow* window)
{
    if (m_window == window) {
        return;
    }
    m_window = window;
    update();
}

QQuickWindow* cwRenderTextBillboards::window() const
{
    return m_window;
}

void cwRenderTextBillboards::setStyle(const Style& style)
{
    if (m_style == style) {
        return;
    }
    m_style = style;

    // Re-lay out every cached string with the new font. Labels share their
    // CachedLayout, so they all pick up the new layout and size.
    for (auto it = m_layouts.begin(); it != m_layouts.end(); ++it) {
        CachedLayout& cached = *it.value();
        cached.layout = makeLayout(it.key(), m_style.font, &cached.size);
    }
    update();
}

cwRenderTextBillboards::Style cwRenderTextBillboards::style() const
{
    return m_style;
}

std::shared_ptr<cwRenderTextBillboards::CachedLayout> cwRenderTextBillboards::acquireLayout(const QString& text)
{
    auto it = m_layouts.constFind(text);
    if (it != m_layouts.constEnd()) {
        return it.value();
    }

    auto cached = std::make_shared<CachedLayout>();
    cached->layout = makeLayout(text, m_style.font, &cached->size);
    m_layouts.insert(text, cached);
    return cached;
}

void cwRenderTextBillboards::releaseLayout(const QString& text)
{
    // Only the cache holds it once the last label showing this string let go.
    auto it = m_layouts.find(text);
    if (it != m_layouts.end() && it.value().use_count() == 1) {
        m_layouts.erase(it);
    }
}

cwBillboardId cwRenderTextBillboards::addBillboard(const TextBillboard& billboard)
{
    const cwBillboardId id = cwBillboardId{m_nextId++};
    m_billboards.emplace(id, Entry{billboard, acquireLayout(billboard.text)});
    update();
    return id;
}

void cwRenderTextBillboards::updateBillboard(cwBillboardId id, const TextBillboard& billboard)
{
    auto it = m_billboards.find(id);
    if (it == m_billboards.end()) {
        return;
    }

    Entry& entry = it->second;
    if (entry.billboard == billboard) {
        return;
    }

    if (entry.billboard.text != billboard.text) {
        const QString oldText = entry.billboard.text;
        entry.layout = acquireLayout(billboard.text);
        releaseLayout(oldText);
    }
    entry.billboard = billboard;
    update();
}

void cwRenderTextBillboards::removeBillboard(cwBillboardId id)
{
    auto it = m_billboards.find(id);
    if (it == m_billboards.end()) {
        return;
    }

    const QString text = it->second.billboard.text;
    m_billboards.erase(it);
    releaseLayout(text);
    update();
}

bool cwRenderTextBillboards::hasBillboard(cwBillboardId id) const
{
    return m_billboards.find(id) != m_billboards.end();
}

int cwRenderTextBillboards::billboardCount() const
{
    return int(m_billboards.size());
}

QSizeF cwRenderTextBillboards::textSize(const QString& text)
{
    auto it = m_layouts.constFind(text);
    if (it != m_layouts.constEnd()) {
        return it.value()->size;
    }

    // A string no label shows is measured without caching, so the cache stays
    // bounded by the labels in the layer.
    QSizeF size;
    makeLayout(text, m_style.font, &size);
    return size;
}

QVector<cwRenderTextBillboards::RenderSlot> cwRenderTextBillboards::buildRenderSlots() const
{
    QVector<RenderSlot> renderSlots;
    renderSlots.reserve(int(m_billboards.size()));
    for (const auto& [id, entry] : m_billboards) {
        RenderSlot slot;
        slot.id = id;
        slot.layout = entry.layout->layout;
        slot.contentSize = entry.layout->size;
        slot.worldPosition = entry.billboard.worldPosition;
        slot.depthBias = entry.billboard.depthBias;
        renderSlots.append(slot);
    }
    return renderSlots;
}

cwRHIObject* cwRenderTextBillboards::createRHIObject()
{
    return new cwRHITextBillboards();
}
