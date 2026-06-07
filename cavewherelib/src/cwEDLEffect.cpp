/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwEDLEffect.h"

#include <algorithm>
#include <cmath>

cwEDLEffect::cwEDLEffect()
{
}

cwEDLEffect::~cwEDLEffect()
{
    delete m_srb;
    delete m_pipeline;
    delete m_layout;
    delete m_sampler;
    delete m_edlUBO;
}

void cwEDLEffect::initialize(QRhi* rhi,
                             QRhiRenderPassDescriptor* outputRPDesc,
                             int outputSampleCount,
                             QRhiBuffer* globalUBO)
{
    if (!rhi || !outputRPDesc || !globalUBO) {
        return;
    }

    m_outputSampleCount = outputSampleCount > 0 ? outputSampleCount : 1;

    m_rhi = rhi;
    m_globalUBO = globalUBO;

    // Detect Y-axis convention for the offscreen texture sampling. Metal,
    // Vulkan, and D3D store textures top-left (Y-down); OpenGL stores them
    // bottom-left (Y-up). The shader uses textureYFlip to compensate.
    m_uniformData.textureYFlip = rhi->isYUpInFramebuffer() ? 0.0f : 1.0f;
    m_uniformData.silhouetteStrength = m_silhouetteStrength;
    m_uniformData.silhouetteClamp = m_silhouetteClamp;
    m_uniformsDirty = true;

    if (!m_edlUBO) {
        const quint32 size = rhi->ubufAligned(sizeof(EdlUniform));
        m_edlUBO = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, size);
        m_edlUBO->create();
        m_uniformsDirty = true;
    }

    if (!m_sampler) {
        // Nearest filtering for depth — bilinear interpolation across a depth
        // edge produces midpoint values that defeat the EDL comparison.
        m_sampler = rhi->newSampler(QRhiSampler::Nearest,
                                    QRhiSampler::Nearest,
                                    QRhiSampler::None,
                                    QRhiSampler::ClampToEdge,
                                    QRhiSampler::ClampToEdge);
        m_sampler->create();
    }

    if (!m_pipeline) {
        createPipeline(outputRPDesc);
    }
}

bool cwEDLEffect::createPipeline(QRhiRenderPassDescriptor* outputRPDesc)
{
    // GlobalBlock (binding 0) is read by both stages — vertex uses
    // projectionMatrix to detect backend Y-flip, fragment uses viewportSize
    // and devicePixelRatio for the sample radius. Stage flags must match
    // shader usage or the bind silently feeds zeros to the missing stage.
    const auto bothStages =
        QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;
    m_layout = m_rhi->newShaderResourceBindings();
    m_layout->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, bothStages, nullptr),
        QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::FragmentStage, nullptr),
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, nullptr, nullptr),
        QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage, nullptr, nullptr),
    });
    m_layout->create();

    QShader vs = cwRHIObject::loadShader(QStringLiteral(":/shaders/EDL.vert.qsb"));
    QShader fs = cwRHIObject::loadShader(QStringLiteral(":/shaders/EDL.frag.qsb"));

    m_pipeline = m_rhi->newGraphicsPipeline();
    m_pipeline->setShaderStages({
        { QRhiShaderStage::Vertex, vs },
        { QRhiShaderStage::Fragment, fs },
    });

    // No vertex buffer — gl_VertexIndex drives the fullscreen triangle.
    m_pipeline->setVertexInputLayout({});
    m_pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    // Depth test off — the swap-chain depth at composite time is the clear
    // value (Background pass doesn't write depth), so any cloud depth passes.
    // Depth write on — the fragment shader writes gl_FragDepth = cloud depth
    // so subsequent Opaque draws can z-test against the cloud surface.
    m_pipeline->setDepthTest(false);
    m_pipeline->setDepthWrite(true);
    m_pipeline->setCullMode(QRhiGraphicsPipeline::None);
    // MUST match the destination render target's sample count. Hardcoding 1
    // here while rendering into a 4× MSAA swap-chain produced cloud pixels at
    // 25% brightness / 25% alpha (only sample 0 was written, MSAA resolve
    // averaged in three transparent-black samples).
    m_pipeline->setSampleCount(m_outputSampleCount);

    // Premultiplied-alpha blend: keep Scene-layer content where the cloud is
    // transparent (alpha=0). Source RGB is already premultiplied in the shader.
    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::One;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    m_pipeline->setTargetBlends({ blend });

    m_pipeline->setShaderResourceBindings(m_layout);
    m_pipeline->setRenderPassDescriptor(outputRPDesc);
    return m_pipeline->create();
}

bool cwEDLEffect::ensureBindings(QRhiTexture* color, QRhiTexture* depth)
{
    if (!color || !depth || !m_globalUBO || !m_edlUBO || !m_sampler || !m_layout) {
        return false;
    }

    if (m_srb && m_lastColor == color && m_lastDepth == depth) {
        return true;
    }

    delete m_srb;
    const auto bothStages =
        QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;
    m_srb = m_rhi->newShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, bothStages, m_globalUBO),
        QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::FragmentStage, m_edlUBO),
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, color, m_sampler),
        QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage, depth, m_sampler),
    });
    if (!m_srb->create()) {
        delete m_srb;
        m_srb = nullptr;
        return false;
    }

    Q_ASSERT(m_layout->isLayoutCompatible(m_srb));

    m_lastColor = color;
    m_lastDepth = depth;
    return true;
}

void cwEDLEffect::updateFrameUniforms(const FrameUniformContext& ctx)
{
    // Skip the recompute when nothing relevant changed — the UBO retains the
    // last value, so we only pay the upload on projection/viewport/DPR edits.
    const bool projectionChanged = !m_frameStateSeen
                                   || ctx.projectionMatrix != m_lastProjectionMatrix;
    const bool viewportChanged = !m_frameStateSeen
                                 || ctx.viewportSize != m_lastViewportSize
                                 || ctx.devicePixelRatio != m_lastDevicePixelRatio;
    if (!projectionChanged && !viewportChanged) {
        return;
    }
    m_frameStateSeen = true;
    m_lastProjectionMatrix = ctx.projectionMatrix;
    m_lastViewportSize = ctx.viewportSize;
    m_lastDevicePixelRatio = ctx.devicePixelRatio;

    if (projectionChanged) {
        // Mirrors linearDepthFromNear() in EDL.frag — see the shader for the
        // derivation. QMatrix4x4::operator()(row, col) maps to GLSL's
        // projectionMatrix[col][row].
        const float a = ctx.projectionMatrix(2, 2);
        const float b = ctx.projectionMatrix(2, 3);
        const bool isPerspective = ctx.projectionMatrix(3, 2) < -0.5f;

        auto depthFromNear = [&](float d) {
            if (isPerspective) {
                return -b * d / ((d + a) * a);
            }
            return -d / a;
        };

        const float midZ = depthFromNear(0.5f);
        const float farZ = depthFromNear(1.0f);
        const float normalizer = std::log2(farZ / std::max(midZ, 1e-6f));
        m_uniformData.strength = m_strengthBaseline / std::max(normalizer, 1.0f);
    }

    if (viewportChanged && ctx.viewportSize.isValid()) {
        const float scale = m_radiusPx * ctx.devicePixelRatio;
        m_uniformData.sampleOffset[0] = scale / float(ctx.viewportSize.width());
        m_uniformData.sampleOffset[1] = scale / float(ctx.viewportSize.height());
    }

    m_uniformsDirty = true;
}

void cwEDLEffect::apply(QRhiCommandBuffer* cb,
                        QRhiTexture* inputColor,
                        QRhiTexture* inputDepth,
                        QSize outputSize)
{
    if (!cb || !m_pipeline) {
        return;
    }

    if (!ensureBindings(inputColor, inputDepth)) {
        return;
    }

    if (m_uniformsDirty) {
        auto* batch = m_rhi->nextResourceUpdateBatch();
        batch->updateDynamicBuffer(m_edlUBO, 0, sizeof(EdlUniform), &m_uniformData);
        cb->resourceUpdate(batch);
        m_uniformsDirty = false;
    }

    cb->setGraphicsPipeline(m_pipeline);
    cb->setShaderResources(m_srb);
    cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));
    cb->draw(3);
}
