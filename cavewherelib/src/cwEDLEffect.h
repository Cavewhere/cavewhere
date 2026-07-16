/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEDLEFFECT_H
#define CWEDLEFFECT_H

// Our includes
#include "cwRHIObject.h"
#include "cwEdlParametersData.h"
#include "cwTracked.h"

// Qt includes
#include <rhi/qrhi.h>
#include <QSize>

/**
 * @brief Eye-Dome Lighting composite.
 *
 * The final pass that writes the swap chain. Reads three offscreen textures:
 * the scene color (Background + Opaque), the point-cloud color, and the depth
 * buffer they share (combined cloud+scene depth). Outputs the scene where there
 * is no cloud and the depth-shaded cloud where there is, with a CloudCompare /
 * Potree log-space 8-tap response: a far-side shape term for the interior depth
 * cue plus a near-side silhouette term that darkens cloud edges against anything
 * more distant behind them — scene geometry, the background, or another cloud
 * surface — so every edge outlines consistently.
 *
 * Bimodal on the offscreen sample count (inputSampleCount on initialize()):
 * > 1 loads EDL_MSAA.frag.qsb and runs per-sample over multisample textures so
 * the swap-chain resolve anti-aliases the result; == 1 loads EDL.frag.qsb and
 * runs the plain 1x path. The pipeline is rebuilt if the mode changes.
 */
class cwEDLEffect : public cwRhiPostProcessEffect {
public:
    cwEDLEffect();
    ~cwEDLEffect() override;

    void initialize(QRhi* rhi,
                    QRhiRenderPassDescriptor* outputRPDesc,
                    int outputSampleCount,
                    QRhiBuffer* globalUBO,
                    quint32 globalUBOStride,
                    int inputSampleCount) override;
    // Drop the cached SRB and texture-pointer identities so the next apply()
    // rebuilds bindings. cwRhiScene calls this when the offscreen textures are
    // recreated (resize) while the effect is preserved — otherwise ensureBindings
    // could compare new texture pointers against freed ones that the allocator
    // reused at the same address and wrongly skip the rebuild.
    void resize(QSize outputSize) override;
    void updateFrameUniforms(const FrameUniformContext& ctx) override;
    void apply(QRhiCommandBuffer* cb,
               QRhiTexture* sceneColor,
               QRhiTexture* cloudColor,
               QRhiTexture* depth,
               QSize outputSize,
               quint32 cameraUniformOffset) override;

    // Live tuning, fed each synchroize() from cwScene::edl(). Staged on a
    // cwTracked; the derived UBO values are recomputed in updateFrameUniforms()
    // only when it reports a change, mirroring how cwRHIPointCloud consumes its
    // tracked inputs.
    void setParameters(const EdlParametersData& parameters);

private:
    // std140 layout: float, float, vec2 (8-byte aligned at offset 8), float
    // (offset 16) = 20 bytes, block-padded to 32. strength is the slope of the
    // shared darkening transfer function; maxDarken is its ceiling.
    struct EdlUniform {
        float strength = 1.0f;
        float textureYFlip = 0.0f;
        float sampleOffset[2] = {0.0f, 0.0f};
        float maxDarken = 0.0f;
    };

    bool createPipeline(QRhiRenderPassDescriptor* outputRPDesc);
    bool ensureBindings(QRhiTexture* sceneColor, QRhiTexture* cloudColor, QRhiTexture* depth);

    // Recompute sampleOffset from m_radiusPx and the last-seen viewport/DPR.
    // No-op until a valid frame state has been seen.
    void recomputeSampleOffset();

    QRhi* m_rhi = nullptr;
    QRhiBuffer* m_globalUBO = nullptr;       // owned by cwRhiScene
    quint32 m_globalUBOStride = 0;           // per-slot stride for the dynamic offset
    int m_outputSampleCount = 1;
    // Sample count of the offscreen textures this effect reads. > 1 selects the
    // per-sample EDL_MSAA.frag path; 1 the plain EDL.frag path.
    int m_inputSampleCount = 1;

    // CPU-side knobs (see EdlUniform / EDL.frag). m_parameters.strength is the
    // *baseline* slope of the shared darkening transfer function; the effective
    // slope sent to the shader is strength / m_logDepthNormalizer, dividing out the
    // projection's log-depth span so ortho and perspective produce a matching EDL
    // response from the same baseline (perspective's neighbor depth-ratios are
    // inherently larger, so it needs less gain). maxDarken is the darkening
    // ceiling; radiusPx is the neighbor sample spread. Fed via setParameters();
    // updateFrameUniforms() re-derives the UBO values when it reports a change.
    cwTracked<EdlParametersData> m_parameters;

    QMatrix4x4 m_lastProjectionMatrix;
    QSize m_lastViewportSize;
    float m_lastDevicePixelRatio = 0.0f;
    bool m_frameStateSeen = false;

    // log2 depth-span normalizer from the last projection, cached so a live
    // strength change recomputes the effective slope without waiting for the next
    // projection change. 1.0 for orthographic; larger for perspective.
    float m_logDepthNormalizer = 1.0f;

    QRhiBuffer* m_edlUBO = nullptr;
    QRhiSampler* m_sampler = nullptr;        // Nearest+Clamp — depth must not bilinear-blend
    QRhiGraphicsPipeline* m_pipeline = nullptr;
    QRhiShaderResourceBindings* m_layout = nullptr;
    QRhiShaderResourceBindings* m_srb = nullptr;

    QRhiTexture* m_lastSceneColor = nullptr;
    QRhiTexture* m_lastCloudColor = nullptr;
    QRhiTexture* m_lastDepth = nullptr;

    EdlUniform m_uniformData;
    bool m_uniformsDirty = true;
};

#endif // CWEDLEFFECT_H
