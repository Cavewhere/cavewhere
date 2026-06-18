#include "cwRenderBillboards.h"
#include "cwQuickItemSubscene.h"
#include "cwItem2DRenderer.h"
#include "cwRHIObject.h"
#include "cwRhiItemRenderer.h"

//Qt includes
#include <QQuickItem>
#include <QQuickWindow>
#include <QMatrix4x4>
#include <QVector4D>

//Std includes
#include <unordered_map>

namespace {

// World units per content pixel for WorldSized billboards: the quad scales with
// the 3D scene, like a QtQuick3D Item2D under a Node.
constexpr float kWorldPerPixel = 0.02f;

// Floors guarding the screen-constant size division: clamp the perspective w and
// the projection scales away from zero so a billboard crossing the eye plane (or
// a degenerate projection) can't produce an infinite scale.
constexpr float kMinClipW = 1e-4f;
constexpr float kMinProjectionScale = 1e-6f;

// Build the billboard's model matrix. Columns 0/1/2 are the camera basis (from
// the view matrix rows) so the quad faces the camera; column 1 is negated so the
// item's y-down pixel space maps to world-down. Column 3 places it at the world
// position (biased toward @a eye so a billboard sitting on a wall isn't
// self-occluded). The trailing translate centers the item on that position. The
// eye position is frame-constant, so the caller computes it once per frame.
QMatrix4x4 buildModelMatrix(const cwRenderBillboards::RenderSlot& slot,
                            const QMatrix4x4& view,
                            const QMatrix4x4& projection,
                            const QMatrix4x4& viewProjection,
                            const QSizeF& logicalViewport,
                            const QVector3D& eye)
{
    const QVector3D right(view(0, 0), view(0, 1), view(0, 2));
    const QVector3D up(view(1, 0), view(1, 1), view(1, 2));
    const QVector3D forward(view(2, 0), view(2, 1), view(2, 2));

    QVector3D position = slot.worldPosition;
    if (slot.depthBias != 0.0f) {
        const QVector3D toEye = eye - position;
        if (!toEye.isNull()) {
            position += toEye.normalized() * slot.depthBias;
        }
    }

    float scaleX = kWorldPerPixel;
    float scaleY = kWorldPerPixel;
    if (slot.sizeMode == cwRenderBillboards::SizeMode::ScreenConstant) {
        // Invert PointCloud.vert's world-radius -> pixels projection so one
        // content pixel maps to one logical screen pixel at this depth. Use the
        // unbiased position's w so the size doesn't drift with depthBias.
        const QVector4D clip = viewProjection * QVector4D(slot.worldPosition, 1.0f);
        const float w = qMax(qAbs(clip.w()), kMinClipW);
        const float projX = qMax(qAbs(projection(0, 0)), kMinProjectionScale);
        const float projY = qMax(qAbs(projection(1, 1)), kMinProjectionScale);
        scaleX = 2.0f * w / (projX * float(logicalViewport.width()));
        scaleY = 2.0f * w / (projY * float(logicalViewport.height()));
    }

    QMatrix4x4 model;
    model.setColumn(0, QVector4D(right * scaleX, 0.0f));
    model.setColumn(1, QVector4D(-up * scaleY, 0.0f));
    model.setColumn(2, QVector4D(forward, 0.0f));
    model.setColumn(3, QVector4D(position, 1.0f));
    model.translate(float(-slot.contentSize.width() / 2.0),
                    float(-slot.contentSize.height() / 2.0),
                    0.0f);
    return model;
}

// Render-thread backend. Keeps one cwItem2DRenderer per slot (each caches its
// own scene-graph render list, matching QtQuick3D) and records each billboard's
// 2D draw inline in the open Overlay pass, so the hardware depth test occludes
// it against cave geometry and the point cloud.
class cwRHIBillboards : public cwRHIObject
{
public:
    void initialize(const ResourceUpdateData&) override {}
    void updateResources(const ResourceUpdateData&) override {}
    void render(const RenderData&) override {}

    void synchronize(const SynchronizeData& data) override
    {
        auto* renderObject = static_cast<cwRenderBillboards*>(data.object);

        QQuickWindow* window = renderObject->window();
        if (window != m_window) {
            m_window = window;
            m_renderers.clear();  // renderers are bound to a window's render context
        }

        // Snapshot plain data while the GUI thread is blocked; nothing past this
        // point dereferences a QQuickItem on the render thread.
        m_slots = renderObject->buildRenderSlots();

        // One renderer per billboard id, kept across frames so each caches its
        // render list. Keying by the stable id (not array position) keeps a
        // renderer bound to its content when other billboards are added/removed.
        // Rebuild by moving survivors into a fresh map in a single pass; renderers
        // whose id is gone are left behind and destroyed with the old map.
        std::unordered_map<cwBillboardId, std::unique_ptr<cwItem2DRenderer>> renderers;
        renderers.reserve(m_slots.size());
        for (const cwRenderBillboards::RenderSlot& slot : m_slots) {
            auto existing = m_renderers.find(slot.id);
            if (existing != m_renderers.end()) {
                renderers.emplace(slot.id, std::move(existing->second));
            } else if (m_window) {
                renderers.emplace(slot.id, std::make_unique<cwItem2DRenderer>(m_window));
            }
        }
        m_renderers = std::move(renderers);
    }

    bool gather(const GatherContext& context, QVector<PipelineBatch>& batches) override
    {
        if (context.renderPass != RenderPass::Overlay || m_slots.isEmpty()) {
            return false;
        }

        const RenderData& renderData = *context.renderData;
        cwRhiItemRenderer* renderer = renderData.renderer;
        if (!renderer) {
            return false;
        }

        QRhiRenderTarget* target = renderer->renderTarget();
        if (!renderer->rhi() || !target) {
            return false;
        }

        const QMatrix4x4 view = renderer->viewMatrix();
        const QMatrix4x4 projection = renderer->projectionMatrix();
        const QMatrix4x4 viewProjection = renderer->viewProjectionMatrix();
        const QVector3D eye = view.inverted().column(3).toVector3D();
        const QSize pixelSize = target->pixelSize();
        // The scene's device pixel ratio (camera/cwRhiScene), NOT the
        // QRhiRenderTarget's — the latter defaults to 1.0, which would size
        // ScreenConstant billboards at half their pixels on a retina display.
        const float devicePixelRatio = renderer->devicePixelRatio();
        const QSizeF logicalViewport(pixelSize.width() / devicePixelRatio,
                                     pixelSize.height() / devicePixelRatio);

        bool appended = false;
        for (int i = 0; i < m_slots.size(); ++i) {
            const cwRenderBillboards::RenderSlot& slot = m_slots.at(i);
            // The content's node subtree may not exist on the first frame or two
            // after it's referenced; skip the slot until it materializes.
            if (slot.rootNodeHandle == 0) {
                continue;
            }

            auto rendererIt = m_renderers.find(slot.id);
            if (rendererIt == m_renderers.end() || !rendererIt->second) {
                continue;
            }
            cwItem2DRenderer* renderer2d = rendererIt->second.get();

            const QMatrix4x4 model = buildModelMatrix(slot, view, projection,
                                                       viewProjection, logicalViewport, eye);

            // viewProjectionMatrix() is already clip-space corrected (matching the
            // convention the cave geometry was rendered with), so the billboard's
            // depth is comparable for the hardware depth test.
            cwItem2DRenderer::Frame frame;
            frame.mvp = viewProjection * model;
            frame.rt = target;
            frame.rpd = renderData.renderPassDescriptor;
            frame.cb = renderData.cb;
            frame.deviceRect = pixelSize;
            frame.devicePixelRatio = devicePixelRatio;
            frame.rootNodeHandle = slot.rootNodeHandle;

            // prepare() runs here — before cwRhiScene opens any render pass — so
            // the inline renderer's resource uploads happen outside a begin/endPass
            // (RHI forbids resource updates mid-pass).
            if (!renderer2d->prepare(frame)) {
                continue;
            }

            PipelineState state;
            state.pipeline = nullptr;
            state.sortKey = makeSortKey(context.objectOrder, nullptr);
            auto& batch = acquirePipelineBatch(batches, state);

            Drawable drawable;
            drawable.type = Drawable::Type::Custom;
            drawable.customDraw = [renderer2d](const RenderData&) {
                // Records the 2D draw into the already-open Overlay pass.
                renderer2d->render();
            };
            batch.drawables.append(drawable);
            appended = true;
        }
        return appended;
    }

private:
    QPointer<QQuickWindow> m_window;
    QVector<cwRenderBillboards::RenderSlot> m_slots;
    std::unordered_map<cwBillboardId, std::unique_ptr<cwItem2DRenderer>> m_renderers;
};

} // namespace

cwRenderBillboards::cwRenderBillboards(QObject* parent)
    : cwRenderObject(parent)
{
}

cwRenderBillboards::~cwRenderBillboards() = default;

void cwRenderBillboards::setWindow(QQuickWindow* window)
{
    if (m_window == window) {
        return;
    }
    m_window = window;
    // Subscenes are bound to a window's scene graph, so recreate them all.
    for (auto& [id, entry] : m_billboards) {
        entry.subscene = makeSubscene(entry.billboard.content);
    }
    update();
}

QQuickWindow* cwRenderBillboards::window() const
{
    return m_window;
}

std::unique_ptr<cwQuickItemSubscene> cwRenderBillboards::makeSubscene(QQuickItem* content) const
{
    if (!content || !m_window) {
        return nullptr;
    }
    return std::make_unique<cwQuickItemSubscene>(content, m_window);
}

cwBillboardId cwRenderBillboards::addBillboard(const Billboard& billboard)
{
    const cwBillboardId id = cwBillboardId{m_nextId++};
    m_billboards.emplace(id, Entry{billboard, makeSubscene(billboard.content)});
    update();
    return id;
}

void cwRenderBillboards::updateBillboard(cwBillboardId id, const Billboard& billboard)
{
    auto it = m_billboards.find(id);
    if (it == m_billboards.end()) {
        return;
    }

    Billboard& current = it->second.billboard;
    if (current.content == billboard.content
        && current.worldPosition == billboard.worldPosition
        && current.sizeMode == billboard.sizeMode
        && current.depthBias == billboard.depthBias) {
        return;  // nothing changed — cheap to re-push an unchanged billboard
    }

    if (current.content != billboard.content) {
        it->second.subscene = makeSubscene(billboard.content);
    }
    current = billboard;
    update();
}

void cwRenderBillboards::removeBillboard(cwBillboardId id)
{
    if (m_billboards.erase(id) > 0) {  // also destroys the subscene, dereferencing the item
        update();
    }
}

bool cwRenderBillboards::hasBillboard(cwBillboardId id) const
{
    return m_billboards.find(id) != m_billboards.end();
}

int cwRenderBillboards::billboardCount() const
{
    return int(m_billboards.size());
}

QVector<cwRenderBillboards::RenderSlot> cwRenderBillboards::buildRenderSlots() const
{
    QVector<RenderSlot> renderSlots;
    renderSlots.reserve(int(m_billboards.size()));
    for (const auto& [id, entry] : m_billboards) {
        QQuickItem* content = entry.billboard.content;
        if (!content || !entry.subscene) {
            continue;
        }

        // The subscene draws the content via its render node, which ignores the
        // item's visible property — so an invisible content item (e.g. a
        // completed, unselected lead marker) would still be drawn. Honour
        // visibility here. Safe to read on the render thread: buildRenderSlots
        // runs from synchronize() while the GUI thread is blocked.
        if (!content->isVisible()) {
            continue;
        }

        RenderSlot slot;
        slot.id = id;
        slot.rootNodeHandle = entry.subscene->rootNodeHandle();
        slot.contentSize = QSizeF(content->width(), content->height());
        slot.worldPosition = entry.billboard.worldPosition;
        slot.sizeMode = entry.billboard.sizeMode;
        slot.depthBias = entry.billboard.depthBias;
        renderSlots.append(slot);
    }
    return renderSlots;
}

cwRHIObject* cwRenderBillboards::createRHIObject()
{
    return new cwRHIBillboards();
}
