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
                             QRhiBuffer* globalUBO,
                             quint32 globalUBOStride,
                             int inputSampleCount)
{
    if (!rhi || !outputRPDesc || !globalUBO) {
        return;
    }

    const int newInputSampleCount = inputSampleCount > 1 ? inputSampleCount : 1;
    // The shader variant is selected by 1x-vs-MSAA, not the exact sample count.
    const bool variantChanged = (newInputSampleCount > 1) != (m_inputSampleCount > 1);

    m_outputSampleCount = outputSampleCount > 0 ? outputSampleCount : 1;
    m_inputSampleCount = newInputSampleCount;

    m_rhi = rhi;
    m_globalUBO = globalUBO;
    m_globalUBOStride = globalUBOStride;

    const auto discardPipeline = [this]() {
        // Leave m_pipeline null so apply()'s guard skips the composite cleanly
        // rather than binding an uncreated pipeline. Drop the layout too so a
        // later retry (createPipeline allocates it unconditionally) can't leak.
        // Used both to reset before the unconditional rebuild below and to bail
        // out cleanly on a failed init step.
        delete m_pipeline;
        m_pipeline = nullptr;
        delete m_layout;
        m_layout = nullptr;
    };

    // Detect Y-axis convention for the offscreen texture sampling. Metal,
    // Vulkan, and D3D store textures top-left (Y-down); OpenGL stores them
    // bottom-left (Y-up). The shader uses textureYFlip to compensate.
    m_uniformData.textureYFlip = rhi->isYUpInFramebuffer() ? 0.0f : 1.0f;
    m_uniformData.strength = m_parameters.value().strength / m_logDepthNormalizer;
    m_uniformData.maxDarken = m_parameters.value().maxDarken;
    m_uniformsDirty = true;

    // initialize() runs on first use and again whenever the sample count changes
    // (cwRhiScene re-inits when the offscreen count differs). Always rebuild the
    // pipeline so it matches the current output rpDesc + sample count and the 1x
    // / MSAA shader variant — a stale pipeline whose sample count differs from
    // the swap chain fails to render.
    discardPipeline();

    if (variantChanged) {
        // The sampleOffset field changes units between 1x and MSAA (UV step vs.
        // texel radius), so force a full uniform recompute, and drop the SRB so
        // it rebinds (the offscreen textures were recreated too).
        delete m_srb;
        m_srb = nullptr;
        m_lastSceneColor = nullptr;
        m_lastCloudColor = nullptr;
        m_lastDepth = nullptr;
        m_frameStateSeen = false;
    }

    if (!m_edlUBO) {
        const quint32 size = rhi->ubufAligned(sizeof(EdlUniform));
        m_edlUBO = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, size);
        if (!m_edlUBO->create()) {
            discardPipeline();
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
            discardPipeline();
            return;
        }
    }

    if (!createPipeline(outputRPDesc)) {
        discardPipeline();
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
    // MSAA variant samples the multisample offscreen per-sample (sampler2DMS +
    // gl_SampleID); 1x variant uses the plain sampler2D path.
    const QShader fs = cwRHIObject::loadShader(
        m_inputSampleCount > 1 ? QStringLiteral(":/shaders/EDL_MSAA.frag.qsb")
                               : QStringLiteral(":/shaders/EDL.frag.qsb"));
    if (!vs.isValid() || !fs.isValid()) {
        return false;
    }

    const auto bothStages =
        QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;
    m_layout = m_rhi->newShaderResourceBindings();
    m_layout->setBindings({
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(0, bothStages, nullptr, m_globalUBOStride),
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
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(0, bothStages, m_globalUBO, m_globalUBOStride),
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
    // Recompute only the terms whose inputs changed: the normalizer on a
    // projection change, the effective slope on a projection or parameter change,
    // the sample offset on a viewport/DPR or parameter change, the ceiling on a
    // parameter change.
    const bool projectionChanged = !m_frameStateSeen
                                   || ctx.projectionMatrix != m_lastProjectionMatrix;
    const bool viewportChanged = !m_frameStateSeen
                                 || ctx.viewportSize != m_lastViewportSize
                                 || ctx.devicePixelRatio != m_lastDevicePixelRatio;
    const bool parametersChanged = m_parameters.isChanged();
    if (!projectionChanged && !viewportChanged && !parametersChanged) {
        return;
    }
    m_frameStateSeen = true;
    m_lastProjectionMatrix = ctx.projectionMatrix;
    m_lastViewportSize = ctx.viewportSize;
    m_lastDevicePixelRatio = ctx.devicePixelRatio;

    if (projectionChanged) {
        // Mirrors linearDepthFromNear() in EDL.frag — see the shader for the
        // derivation. QMatrix4x4::operator()(row, col) maps to GLSL's
        // projectionMatrix[col][row]. The log2 span of the far half of the depth
        // range normalizes the baseline so ortho and perspective match: it is 1
        // for orthographic (linear depth) and large for perspective.
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
        const float normalizer = std::log2(farZ / (std::max)(midZ, 1e-6f));
        m_logDepthNormalizer = (std::max)(normalizer, 1.0f);
    }

    // Effective slope is the baseline divided by the per-projection normalizer,
    // so it tracks both projection and parameter changes.
    if (projectionChanged || parametersChanged) {
        m_uniformData.strength = m_parameters.value().strength / m_logDepthNormalizer;
    }

    if (viewportChanged || parametersChanged) {
        recomputeSampleOffset();
    }

    if (parametersChanged) {
        m_uniformData.maxDarken = m_parameters.value().maxDarken;
    }

    m_parameters.resetChanged();
    m_uniformsDirty = true;
}

void cwEDLEffect::recomputeSampleOffset()
{
    if (!m_lastViewportSize.isValid()) {
        return;
    }
    const float scale = m_parameters.value().radiusPx * m_lastDevicePixelRatio;
    if (m_inputSampleCount > 1) {
        // MSAA path: EDL_MSAA.frag offsets neighbors in integer texels off
        // gl_FragCoord, so the radius stays in device pixels (no UV division).
        // sampleOffset.x carries it; .y is unused.
        m_uniformData.sampleOffset[0] = scale;
        m_uniformData.sampleOffset[1] = 0.0f;
        return;
    }
    m_uniformData.sampleOffset[0] = scale / float(m_lastViewportSize.width());
    m_uniformData.sampleOffset[1] = scale / float(m_lastViewportSize.height());
}

void cwEDLEffect::apply(QRhiCommandBuffer* cb,
                        QRhiTexture* sceneColor,
                        QRhiTexture* cloudColor,
                        QRhiTexture* depth,
                        QSize outputSize,
                        quint32 cameraUniformOffset)
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
    // Binding 0 (the global camera block) is dynamic-offset so the composite
    // linearizes depth with the same camera slot the depth buffer was rendered
    // with (live = 0, offscreen = the per-slot stride).
    const QRhiCommandBuffer::DynamicOffset cameraOffset(0, cameraUniformOffset);
    cb->setShaderResources(m_srb, 1, &cameraOffset);
    cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));
    cb->draw(3);
}

void cwEDLEffect::setParameters(const EdlParametersData& parameters)
{
    // Stage only — cwTracked flags the change; updateFrameUniforms() re-derives
    // the UBO values (which also depend on the projection/viewport) next frame.
    m_parameters.setValue(parameters);
}
