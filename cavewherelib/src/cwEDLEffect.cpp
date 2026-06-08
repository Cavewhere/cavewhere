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

    const auto fail = [this]() {
        // Leave m_pipeline null so apply()'s guard skips the composite cleanly
        // rather than binding an uncreated pipeline. Drop the layout too so a
        // later retry (createPipeline allocates it unconditionally) can't leak.
        delete m_pipeline;
        m_pipeline = nullptr;
        delete m_layout;
        m_layout = nullptr;
    };

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
        if (!m_edlUBO->create()) {
            fail();
            return;
        }
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
        if (!m_sampler->create()) {
            fail();
            return;
        }
    }

    if (!m_pipeline && !createPipeline(outputRPDesc)) {
        fail();
        return;
    }
}

bool cwEDLEffect::createPipeline(QRhiRenderPassDescriptor* outputRPDesc)
{
    // GlobalBlock (binding 0) is read by both stages — vertex uses
    // projectionMatrix to detect backend Y-flip, fragment uses viewportSize
    // and devicePixelRatio for the sample radius. Stage flags must match
    // shader usage or the bind silently feeds zeros to the missing stage.
    const QShader vs = cwRHIObject::loadShader(QStringLiteral(":/shaders/EDL.vert.qsb"));
    const QShader fs = cwRHIObject::loadShader(QStringLiteral(":/shaders/EDL.frag.qsb"));
    if (!vs.isValid() || !fs.isValid()) {
        return false;
    }

    const auto bothStages =
        QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;
    m_layout = m_rhi->newShaderResourceBindings();
    m_layout->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, bothStages, nullptr),
        QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::FragmentStage, nullptr),
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, nullptr, nullptr),
        QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage, nullptr, nullptr),
        QRhiShaderResourceBinding::sampledTexture(4, QRhiShaderResourceBinding::FragmentStage, nullptr, nullptr),
    });
    if (!m_layout->create()) {
        return false;
    }

    m_pipeline = m_rhi->newGraphicsPipeline();
    m_pipeline->setShaderStages({
        { QRhiShaderStage::Vertex, vs },
        { QRhiShaderStage::Fragment, fs },
    });

    // No vertex buffer — gl_VertexIndex drives the fullscreen triangle.
    m_pipeline->setVertexInputLayout({});
    m_pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    // Depth test off — this fullscreen pass writes every swap-chain pixel.
    // Depth write on — the shader writes gl_FragDepth = combined cloud+scene
    // depth so post-composite passes (Transparent, Overlay) z-test correctly
    // against everything already drawn.
    m_pipeline->setDepthTest(false);
    m_pipeline->setDepthWrite(true);
    m_pipeline->setCullMode(QRhiGraphicsPipeline::None);
    // MUST match the destination render target's sample count. Hardcoding 1
    // here while rendering into a 4× MSAA swap-chain produced cloud pixels at
    // 25% brightness / 25% alpha (only sample 0 was written, MSAA resolve
    // averaged in three transparent-black samples).
    m_pipeline->setSampleCount(m_outputSampleCount);

    // No blend: the composite writes the final color (the scene, or the shaded
    // cloud over it) opaquely over the freshly-cleared swap chain. The scene
    // and cloud were already combined in the shader, not via fixed-function
    // blending, because the silhouette darkens the cloud edge before compositing.
    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = false;
    m_pipeline->setTargetBlends({ blend });

    m_pipeline->setShaderResourceBindings(m_layout);
    m_pipeline->setRenderPassDescriptor(outputRPDesc);
    return m_pipeline->create();
}

bool cwEDLEffect::ensureBindings(QRhiTexture* sceneColor, QRhiTexture* cloudColor, QRhiTexture* depth)
{
    if (!sceneColor || !cloudColor || !depth || !m_globalUBO || !m_edlUBO || !m_sampler || !m_layout) {
        return false;
    }

    if (m_srb && m_lastSceneColor == sceneColor && m_lastCloudColor == cloudColor && m_lastDepth == depth) {
        return true;
    }

    delete m_srb;
    const auto bothStages =
        QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;
    m_srb = m_rhi->newShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, bothStages, m_globalUBO),
        QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::FragmentStage, m_edlUBO),
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, cloudColor, m_sampler),
        QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage, depth, m_sampler),
        QRhiShaderResourceBinding::sampledTexture(4, QRhiShaderResourceBinding::FragmentStage, sceneColor, m_sampler),
    });
    if (!m_srb->create()) {
        delete m_srb;
        m_srb = nullptr;
        return false;
    }

    Q_ASSERT(m_layout->isLayoutCompatible(m_srb));

    m_lastSceneColor = sceneColor;
    m_lastCloudColor = cloudColor;
    m_lastDepth = depth;
    return true;
}

void cwEDLEffect::resize(QSize outputSize)
{
    Q_UNUSED(outputSize);
    // Force the next apply() to rebuild the SRB against the recreated textures.
    // Clearing the cached pointer identities is essential: a freed texture can
    // be reallocated at the same address, which would make ensureBindings()'s
    // pointer compare wrongly report "unchanged" and keep the stale SRB.
    delete m_srb;
    m_srb = nullptr;
    m_lastSceneColor = nullptr;
    m_lastCloudColor = nullptr;
    m_lastDepth = nullptr;
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
                        QRhiTexture* sceneColor,
                        QRhiTexture* cloudColor,
                        QRhiTexture* depth,
                        QSize outputSize)
{
    if (!cb || !m_pipeline) {
        return;
    }

    if (!ensureBindings(sceneColor, cloudColor, depth)) {
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
