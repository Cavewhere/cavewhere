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

// Qt includes
#include <rhi/qrhi.h>
#include <QSize>

/**
 * @brief Eye-Dome Lighting post-process effect.
 *
 * Samples the PointCloud pass's offscreen color+depth into the swap-chain
 * pass with depth-aware darkening of silhouette edges. Standard CloudCompare /
 * Potree response: log-space 8-tap depth comparison, premultiplied-alpha
 * composite so transparent (non-cloud) pixels leave the Scene layer intact.
 */
class cwEDLEffect : public cwRhiPostProcessEffect {
public:
    cwEDLEffect();
    ~cwEDLEffect() override;

    void initialize(QRhi* rhi,
                    QRhiRenderPassDescriptor* outputRPDesc,
                    int outputSampleCount,
                    QRhiBuffer* globalUBO) override;
    // No resize() — ensureBindings() detects the new color/depth pointers on
    // the next apply() call and rebuilds the SRB automatically.
    void apply(QRhiCommandBuffer* cb,
               QRhiTexture* inputColor,
               QRhiTexture* inputDepth,
               QSize outputSize) override;

private:
    // std140-compatible: four floats = 16 bytes, satisfies UBO alignment.
    struct EdlUniform {
        float strength = 300.0f;   // CloudCompare-baseline; user strength later multiplies this
        float radiusPx = 1.4f;
        float textureYFlip = 0.0f; // set from rhi->isYUpInFramebuffer() at initialize
        float _pad0 = 0.0f;
    };

    bool createPipeline(QRhiRenderPassDescriptor* outputRPDesc);
    bool ensureBindings(QRhiTexture* color, QRhiTexture* depth);

    QRhi* m_rhi = nullptr;
    QRhiBuffer* m_globalUBO = nullptr;       // owned by cwRhiScene
    int m_outputSampleCount = 1;

    QRhiBuffer* m_edlUBO = nullptr;
    QRhiSampler* m_sampler = nullptr;        // Nearest+Clamp — depth must not bilinear-blend
    QRhiGraphicsPipeline* m_pipeline = nullptr;
    QRhiShaderResourceBindings* m_layout = nullptr;
    QRhiShaderResourceBindings* m_srb = nullptr;

    QRhiTexture* m_lastColor = nullptr;
    QRhiTexture* m_lastDepth = nullptr;

    EdlUniform m_uniformData;
    bool m_uniformsDirty = true;
};

#endif // CWEDLEFFECT_H
