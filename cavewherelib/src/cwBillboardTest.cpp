#include "cwBillboardTest.h"
#include "cwRHIObject.h"
#include "cwRhiItemRenderer.h"
#include "cwScene.h"
#include "cwItem2DRenderer.h"
#include "cwQuickItemSubscene.h"

#include <QQuickWindow>
#include <QMatrix4x4>
#include <QVector4D>
#include <QDebug>

#include <memory>

namespace {

// World units per item pixel. The billboard is world-sized (scales with the 3D
// scene, like a QtQuick3D Item2D under a Node) — constant screen size is a
// refinement for the real implementation.
constexpr float kWorldPerPixel = 0.02f;

const char* kContentQml = R"QML(
import QtQuick
Rectangle {
    width: 180
    height: 56
    color: "#cc2f6e"
    radius: 8
    border.color: "white"
    border.width: 2
    Text {
        anchors.centerIn: parent
        text: "BILLBOARD"
        color: "white"
        font.pixelSize: 22
        font.bold: true
    }
}
)QML";

// Render-thread backend. Drives a cwItem2DRenderer (the public wrapper over the
// QSGRenderer inline path), feeding it the billboard's 3D model-view-projection
// and the cwRhiScene's live render target (shared depth buffer), and records the
// 2D draw inline inside the open Overlay pass — so the hardware depth test
// occludes it against cave geometry.
class cwRHIBillboardTest : public cwRHIObject
{
public:
    void initialize(const ResourceUpdateData&) override {}
    void updateResources(const ResourceUpdateData&) override {}
    void render(const RenderData&) override {}

    void synchronize(const SynchronizeData& data) override
    {
        auto* renderObject = static_cast<cwBillboardTestRender*>(data.object);
        m_subscene = renderObject->subscene;
        m_worldPosition = renderObject->worldPosition;
    }

    bool gather(const GatherContext& context, QVector<PipelineBatch>& batches) override
    {
        if (context.renderPass != RenderPass::Overlay) {
            return false;
        }

        // prepare() runs here — before cwRhiScene opens any render pass — so the
        // inline renderer's resource uploads happen outside a begin/endPass (RHI
        // forbids resource updates mid-pass).
        if (!prepare(*context.renderData)) {
            return false;
        }

        PipelineState state;
        state.pipeline = nullptr;
        state.sortKey = makeSortKey(context.objectOrder, nullptr);

        auto& batch = acquirePipelineBatch(batches, state);

        cwItem2DRenderer* renderer2d = m_renderer2d.get();
        Drawable drawable;
        drawable.type = Drawable::Type::Custom;
        drawable.customDraw = [renderer2d](const RenderData&) {
            // Records the 2D draw into the already-open Overlay pass.
            renderer2d->render();
        };
        batch.drawables.append(drawable);
        return true;
    }

private:
    bool prepare(const RenderData& renderData)
    {
        if (!m_subscene) {
            return false;
        }

        QQuickItem* content = m_subscene->content();
        QQuickWindow* window = m_subscene->window();
        if (!content || !window) {
            return false;
        }

        cwRhiItemRenderer* renderer = renderData.renderer;
        if (!renderer) {
            return false;
        }

        QRhiRenderTarget* target = renderer->renderTarget();
        if (!renderer->rhi() || !target) {
            return false;
        }

        // The content item's scene-graph subtree is built during the window's
        // sync. It may not exist on the very first frame; skip until it does.
        const quintptr rootNodeHandle = m_subscene->rootNodeHandle();
        if (rootNodeHandle == 0) {
            return false;
        }

        if (!m_renderer2d) {
            m_renderer2d = std::make_unique<cwItem2DRenderer>(window);
        }

        // Billboard model matrix: place the item at the world position, oriented
        // to face the camera (columns are the camera basis from the view matrix),
        // scaled from pixels to world units. Column 1 is -up so the item's
        // y-down pixel space maps to world-down.
        const QMatrix4x4 view = renderer->viewMatrix();
        const QVector3D right(view(0, 0), view(0, 1), view(0, 2));
        const QVector3D up(view(1, 0), view(1, 1), view(1, 2));
        const QVector3D forward(view(2, 0), view(2, 1), view(2, 2));

        QMatrix4x4 model;
        model.setColumn(0, QVector4D(right * kWorldPerPixel, 0.0f));
        model.setColumn(1, QVector4D(-up * kWorldPerPixel, 0.0f));
        model.setColumn(2, QVector4D(forward, 0.0f));
        model.setColumn(3, QVector4D(m_worldPosition, 1.0f));
        // Center the item on the world position.
        model.translate(float(-content->width() / 2.0),
                        float(-content->height() / 2.0),
                        0.0f);

        // viewProjectionMatrix() is already clip-space corrected (rhi clip matrix
        // * projection * view), matching the convention the cave geometry was
        // rendered with — so the billboard's depth is comparable.
        cwItem2DRenderer::Frame frame;
        frame.mvp = renderer->viewProjectionMatrix() * model;
        frame.rt = target;
        frame.rpd = renderData.renderPassDescriptor;
        frame.cb = renderData.cb;
        frame.deviceRect = target->pixelSize();
        frame.devicePixelRatio = target->devicePixelRatio();
        frame.rootNodeHandle = rootNodeHandle;

        return m_renderer2d->prepare(frame);
    }

    cwQuickItemSubscene* m_subscene = nullptr;
    QVector3D m_worldPosition;
    std::unique_ptr<cwItem2DRenderer> m_renderer2d;
};

} // namespace

// ---------------------------------------------------------------------------
// cwBillboardTestRender
// ---------------------------------------------------------------------------

cwBillboardTestRender::cwBillboardTestRender(QObject* parent)
    : cwRenderObject(parent)
{
}

cwRHIObject* cwBillboardTestRender::createRHIObject()
{
    return new cwRHIBillboardTest();
}

// ---------------------------------------------------------------------------
// cwBillboardTest
// ---------------------------------------------------------------------------

cwBillboardTest::cwBillboardTest(QQuickItem* parent)
    : QQuickItem(parent)
{
}

cwBillboardTest::~cwBillboardTest() = default;

void cwBillboardTest::setScene(cwScene* scene)
{
    if (m_scene == scene) {
        return;
    }
    m_scene = scene;
    emit sceneChanged();
    tryInitialize();
}

void cwBillboardTest::setWorldPosition(const QVector3D& position)
{
    if (m_worldPosition == position) {
        return;
    }
    m_worldPosition = position;
    if (m_renderObject) {
        m_renderObject->worldPosition = position;
        m_renderObject->update();
    }
    emit worldPositionChanged();
}

void cwBillboardTest::componentComplete()
{
    QQuickItem::componentComplete();
    tryInitialize();
}

void cwBillboardTest::itemChange(ItemChange change, const ItemChangeData& value)
{
    if (change == ItemSceneChange) {
        tryInitialize();
    }
    QQuickItem::itemChange(change, value);
}

void cwBillboardTest::tryInitialize()
{
    if (m_initialized || !isComponentComplete()) {
        return;
    }

    QQuickWindow* quickWindow = window();
    QQmlEngine* engine = qmlEngine(this);
    if (!m_scene || !quickWindow || !engine) {
        return;
    }

    m_component = new QQmlComponent(engine, this);
    m_component->setData(kContentQml, QUrl());
    auto* content = qobject_cast<QQuickItem*>(m_component->create(qmlContext(this)));
    if (!content) {
        qWarning() << "cwBillboardTest: failed to create content item:"
                   << m_component->errorString();
        return;
    }
    content->setParent(this);
    m_content = content;

    // Reference the item into the window's scene graph without showing it in the
    // normal 2D overlay — the public wrapper hides the refWindow /
    // refFromEffectItem private-API dance.
    m_subscene = std::make_unique<cwQuickItemSubscene>(content, quickWindow);

    m_renderObject = new cwBillboardTestRender(this);
    m_renderObject->subscene = m_subscene.get();
    m_renderObject->worldPosition = m_worldPosition;
    m_renderObject->setScene(m_scene);

    m_initialized = true;
}
