#include "cwCompassRenderer.h"
#include "cwCompassBackendItem.h"
#include "cwRHIObject.h"

//Qt includes
#include <rhi/qrhi.h>

cwCompassRenderer::cwCompassRenderer()
{
}

cwCompassRenderer::~cwCompassRenderer()
{
    delete m_vertexBuffer;
    // delete m_textureVertexBuffer;
    delete m_uniformBuffer;

    // delete m_compassTexture;
    // delete m_compassRenderTarget;
    // delete m_compassRenderPassDesc;

    // delete m_shadowTexture;
    // delete m_shadowRenderTarget;
    // delete m_shadowRenderPassDesc;

    // delete m_horizontalBlurTexture;
    // delete m_horizontalBlurRenderTarget;
    // delete m_horizontalBlurRenderPassDesc;

    delete m_compassPipeline;
    delete m_compassBindings;

    // delete m_shadowPipeline;
    // delete m_shadowBindings;

    // delete m_blurPipeline;
    // delete m_blurBindings;

    // delete m_outputPipeline;
    // delete m_outputBindings;
}

void cwCompassRenderer::initialize(QRhiCommandBuffer *cb)
{
    if (!m_resourcesInitialized) {
        initializeResources(cb);
        m_resourcesInitialized = true;
    }
}

void cwCompassRenderer::initializeResources(QRhiCommandBuffer *cb)
{
    auto rhi = cb->rhi();

    // Create uniform buffer
    auto size = rhi->ubufAligned(sizeof(QMatrix4x4));
    m_uniformBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, size);
    m_uniformBuffer->create();

    initializeGeometry(cb);
    createPipelines(rhi);
}

/**
 * @brief cwGLCompass::generateStarGeometry
 * @param triangles
 * @param colors
 */
void cwCompassRenderer::generateStarGeometry(QVector<Vertex> &trianglesPoints,
                                             cwCompassRenderer::Direction direction)
{
    enum class EdgeClass : int {
        Middle,
        Outside
    };

    struct Vectex {
        QVector3D pos;
        EdgeClass edge;
    };

    std::array<Vectex, 6> defaultPoints = {{
        {QVector3D(0.0, 0.0, 0.15), EdgeClass::Middle},   // Middle, small triangle
        {QVector3D(-0.2, 0.2, 0.0), EdgeClass::Outside},  // Outside, small triangle
        {QVector3D(0.0, 0.2, 0.15), EdgeClass::Middle},   // Middle
        {QVector3D(-0.2, 0.2, 0.0), EdgeClass::Outside},  // Outside
        {QVector3D(0.0, 0.2, 0.15), EdgeClass::Middle},   // Middle
        {QVector3D(0.0, 1.0, 0.0), EdgeClass::Middle}    // Outside
    }};

    QMatrix4x4 rotationMatrix;
    rotationMatrix.setToIdentity();

    QVector3D black(0.0, 0.0, 0.0); //Black
    QVector3D white(1.0, 1.0, 1.0); //White
    QVector3D bottowWhite(0.5, 0.5, 0.5); //g

    QVector3D fadeBlack(0.35, 0.35, 0.35);
    QVector3D fadeWhite(0.9, 0.9, 0.9);
    QVector3D currentColor;

    //Which side should be black first
    int firstBlack;

    switch(direction) {
    case Top:
        firstBlack = 0;
        break;
    case Bottom:
        firstBlack = 1;
        break;
    }

    //The top half of the compass rose
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 2; j++) {
            if(j == firstBlack) {
                currentColor = black;
            } else if(direction == Bottom) {
                currentColor = bottowWhite;
            } else {
                currentColor = white;
            }

            for(int p = 0; p < defaultPoints.size(); p++) {
                QVector3D point;

                if(i == 0 && p == 5) {
                    //Make the north more north
                    QVector3D extendendNorth = QVector3D(0.0, 1.5, 0.0);
                    point = rotationMatrix.map(extendendNorth);
                } else {
                    point = rotationMatrix.map(defaultPoints[p].pos);
                }

                QVector3D finalColor = currentColor;
                if(direction == Bottom) {
                    //The bottom of the compass rose is flat
                    point.setZ(0.0);
                } else {

                    //Apply gradient coloring on edges so the compass stands out when on it side
                    if(defaultPoints[p].edge == EdgeClass::Outside) {
                        if(currentColor == black) {
                            finalColor = fadeBlack;
                        } else {
                            finalColor = fadeWhite;
                        }
                    }
                }

                trianglesPoints.append(Vertex{point, finalColor});
            }

            rotationMatrix.scale(-1.0, 1.0, 1.0);
        }
        rotationMatrix.rotate(90.0, 0.0, 0.0, 1.0);
    }
}

void cwCompassRenderer::initializeGeometry(QRhiCommandBuffer *cb)
{
    // Generate compass geometry
    QVector<Vertex> allPoints;
    generateStarGeometry(allPoints, Top);
    generateStarGeometry(allPoints, Bottom);
    m_vertexCount = allPoints.size();

    // Create vertex buffer
    m_vertexBuffer = cb->rhi()->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(Vertex) * allPoints.size());
    m_vertexBuffer->create();

    // Upload vertex data
    QRhiResourceUpdateBatch* batch = cb->rhi()->nextResourceUpdateBatch();
    batch->uploadStaticBuffer(m_vertexBuffer, allPoints.constData());

    // Texture vertex buffer for screen quad
    // struct TexVertex {
    //     QVector2D position;
    // };
    // QVector<TexVertex> textureVertices = {
    //                                       { QVector2D(0.0f, 0.0f) },
    //                                       { QVector2D(0.0f, 1.0f) },
    //                                       { QVector2D(1.0f, 0.0f) },
    //                                       { QVector2D(1.0f, 1.0f) },
    //                                       };

    // m_textureVertexBuffer = cb->rhi()->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(TexVertex) * textureVertices.size());
    // m_textureVertexBuffer->create();

    // batch->uploadStaticBuffer(m_textureVertexBuffer, textureVertices.constData());
    cb->resourceUpdate(batch);
}


void cwCompassRenderer::createPipelines(QRhi* rhi)
{
    // Compass pipeline
    QShader vsCompass = cwRHIObject::loadShader(":/shaders/compass/compass.vert.qsb");
    QShader fsCompass = cwRHIObject::loadShader(":/shaders/compass/compass.frag.qsb");

    Q_ASSERT(vsCompass.isValid());
    Q_ASSERT(fsCompass.isValid());

    Q_ASSERT(m_uniformBuffer);
    m_compassBindings = rhi->newShaderResourceBindings();
    m_compassBindings->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, m_uniformBuffer),
    });
    m_compassBindings->create();

    QRhiGraphicsPipeline::TargetBlend blendState;
    blendState.enable = true;
    blendState.srcColor = QRhiGraphicsPipeline::SrcAlpha;
    blendState.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;

    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({ { sizeof(Vertex) } });
    inputLayout.setAttributes({
                              { 0, 0, QRhiVertexInputAttribute::Float3, offsetof(Vertex, position) },
                              { 0, 1, QRhiVertexInputAttribute::Float3, offsetof(Vertex, color) },
                              });

    m_compassPipeline = rhi->newGraphicsPipeline();
    m_compassPipeline->setShaderStages({ {QRhiShaderStage::Vertex, vsCompass},
                                        {QRhiShaderStage::Fragment, fsCompass} });
    m_compassPipeline->setShaderResourceBindings(m_compassBindings);
    m_compassPipeline->setVertexInputLayout(inputLayout);
    m_compassPipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    m_compassPipeline->setTargetBlends({ blendState });
    m_compassPipeline->setDepthTest(true);
    m_compassPipeline->setDepthWrite(true);
    // m_compassPipeline->setCullMode(QRhiGraphicsPipeline::Back); //FIXME: geometry is setup correctly, culling
    m_compassPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    m_compassPipeline->create();

    // Similarly create pipelines for shadow, blur, and output
    // Load shaders and set up bindings and pipelines for each pass
}

void cwCompassRenderer::synchronize(QQuickRhiItem* item)
{
    cwCompassBackendItem* compassItem = static_cast<cwCompassBackendItem*>(item);
    m_rotation = compassItem->compassRotation();
}

void cwCompassRenderer::render(QRhiCommandBuffer* cb)
{
    // Determine if looking down
    QMatrix4x4 rotationMatrix;
    rotationMatrix.rotate(m_rotation);
    m_lookingDown = rotationMatrix.map(QVector3D(0.0, 0.0, 1.0)).z() > 0.0f;

    QMatrix4x4 orthoMatrix;
    orthoMatrix.ortho(-2.0, 2.0, -2.0, 2.0, -1.5, 1.5);

    //Correct OpenGL projection matrix, clipspace
    orthoMatrix = cb->rhi()->clipSpaceCorrMatrix() * orthoMatrix;

    QMatrix4x4 compassMVP = orthoMatrix * rotationMatrix;

    // QMatrix4x4 modelView;
    // modelView.translate(0.5, 0.5, 0.0);
    // modelView.rotate(m_rotation);
    // modelView.translate(-0.5, -0.5, 0.0);
    // modelView.translate(0.0, 0.0, -0.1);

    QRhiResourceUpdateBatch* resourceUpdates = cb->rhi()->nextResourceUpdateBatch();
    // m_resourceUpdates.append(resourceUpdates);

    // if (!m_resourcesInitialized) {
    //     initializeResources();
    //     m_resourcesInitialized = true;
    // }

    // Update uniform buffer
    resourceUpdates->updateDynamicBuffer(m_uniformBuffer, 0, sizeof(QMatrix4x4), compassMVP.constData());

    // Render passes
    // if (m_lookingDown) {
    //     renderShadow(cb);
    // }
    renderCompass(cb, resourceUpdates);

    // if (m_lookingDown) {
    //     drawFramebuffer(cb, m_shadowTexture, m_outputPipeline, m_outputBindings);
    // }
    // drawFramebuffer(cb, m_compassTexture, m_outputPipeline, m_outputBindings);

    // Execute resource updates
    // cb->resourceUpdate(m_resourceUpdates.takeFirst());
}

void cwCompassRenderer::renderShadow(QRhiCommandBuffer* cb)
{
    // // First pass: Render compass to shadow texture without colors
    // cb->beginPass(m_shadowRenderTarget, QColor(0, 0, 0, 0), { 1.0f, 0 }, m_resourceUpdates.takeFirst());

    // cb->setGraphicsPipeline(m_shadowPipeline);
    // cb->setShaderResources(m_shadowBindings);
    // cb->setViewport(QRhiViewport(0, 0, 512, 512));

    // const QRhiCommandBuffer::VertexInput compassVertexInput(m_vertexBuffer, 0);
    // cb->setVertexInput(0, { compassVertexInput });

    // cb->draw(m_vertexCount);

    // cb->endPass();

    // // Second pass: Horizontal blur
    // cb->beginPass(m_horizontalBlurRenderTarget, QColor(0, 0, 0, 0), { 1.0f, 0 }, nullptr);

    // cb->setGraphicsPipeline(m_blurPipeline);
    // cb->setShaderResources(m_blurBindings);
    // cb->setViewport(QRhiViewport(0, 0, 512, 512));

    // const QRhiCommandBuffer::VertexInput quadVertexInput(m_textureVertexBuffer, 0);
    // cb->setVertexInput(0, { quadVertexInput });

    // // Set blur direction uniform to horizontal
    // // Update uniforms if necessary

    // cb->draw(4);

    // cb->endPass();

    // // Third pass: Vertical blur (render back to shadow texture)
    // cb->beginPass(m_shadowRenderTarget, QColor(0, 0, 0, 0), { 1.0f, 0 }, nullptr);

    // cb->setGraphicsPipeline(m_blurPipeline);
    // cb->setShaderResources(m_blurBindings);
    // cb->setViewport(QRhiViewport(0, 0, 512, 512));

    // // Set blur direction uniform to vertical
    // // Update uniforms if necessary

    // cb->draw(4);

    // cb->endPass();
}

void cwCompassRenderer::renderCompass(QRhiCommandBuffer* cb, QRhiResourceUpdateBatch* batch)
{
    // QRhiResourceUpdateBatch* resourceUpdates = cb->rhi90m_rhi->nextResourceUpdateBatch();

    cb->beginPass(renderTarget(), QColor(0, 0, 0, 0), { 1.0f, 0 }, batch);

    const QSize outputSize = renderTarget()->pixelSize();
    cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

    cb->setGraphicsPipeline(m_compassPipeline);
    cb->setShaderResources();

    const QRhiCommandBuffer::VertexInput compassVertexInput(m_vertexBuffer, 0);
    cb->setVertexInput(0, 1, &compassVertexInput);

    cb->draw(m_vertexCount);

    cb->endPass();

    // Label the compass (since QRhi doesn't support QPainter, we'll handle this differently)
    // labelCompass();
}

void cwCompassRenderer::drawFramebuffer(QRhiCommandBuffer* cb, QRhiTexture* texture, QRhiGraphicsPipeline* pipeline, QRhiShaderResourceBindings* bindings)
{
    // cb->beginPass(m_renderTarget, QColor::fromRgbF(0.33, 0.66, 1.0, 1.0), { 1.0f, 0 }, nullptr);

    // cb->setGraphicsPipeline(pipeline);
    // cb->setShaderResources(bindings);
    // cb->setViewport(QRhiViewport(0, 0, m_renderTarget->pixelSize().width(), m_renderTarget->pixelSize().height()));

    // const QRhiCommandBuffer::VertexInput quadVertexInput(m_textureVertexBuffer, 0);
    // cb->setVertexInput(0, { quadVertexInput });

    // cb->draw(4);

    // cb->endPass();
}

