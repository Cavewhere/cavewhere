/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440 core

// Eye-Dome Lighting (Boucheny 2009 / CloudCompare / Potree). For each pixel,
// compare its depth to a ring of 8 neighbors. Pixels behind a silhouette
// edge (neighbors closer) get darkened in proportion to the depth gap.
//
// Operates in log2(depth) space for stability across the depth range
// (otherwise the EDL response collapses near the far plane where the
// depth buffer is logarithmic).

layout(std140, binding = 0) uniform GlobalBlock {
    mat4 viewProjectionMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    float devicePixelRatio;
    vec2 viewportSize;
};

layout(std140, binding = 1) uniform EdlBlock {
    float strength;        // ~300 baseline scale; CloudCompare default ~1.0 modulates the exponent
    float radiusPx;        // ~1.4 default; sampling radius in logical (pre-DPR) pixels
    // 1.0 when the backend's framebuffer is Y-down (Metal/Vulkan/D3D) so the
    // offscreen texture's V axis follows native top-left convention and we
    // must flip V when sampling; 0.0 on OpenGL (Y-up framebuffer, V=0 is
    // bottom as expected). Set from C++ via QRhi::isYUpInFramebuffer().
    float textureYFlip;
    float _pad0;
};

layout(binding = 2) uniform sampler2D uColor;
layout(binding = 3) uniform sampler2D uDepth;

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 fragColor;

void main()
{
    vec2 uv = (textureYFlip > 0.5) ? vec2(vUV.x, 1.0 - vUV.y) : vUV;

    // Sample depth first and discard background fragments before sampling
    // color — saves the color tap and the 8 EDL depth taps on every empty
    // pixel. discard skips both color blend AND depth write so the swap-chain
    // depth buffer keeps its cleared (far-plane) value where there's no cloud.
    float d0 = texture(uDepth, uv).r;
    if (d0 >= 0.9999) {
        discard;
    }
    vec4 src = texture(uColor, uv);
    if (src.a < 0.001) {
        discard;
    }

    float logD0 = log2(d0 + 1e-6);

    // 8-tap ring of neighbours. Direction vectors are precomputed; radius is
    // scaled by devicePixelRatio so it's perceptually constant across DPR.
    const vec2 dirs[8] = vec2[](
        vec2( 1.0,  0.0), vec2(-1.0,  0.0), vec2( 0.0,  1.0), vec2( 0.0, -1.0),
        vec2( 0.707,  0.707), vec2(-0.707,  0.707),
        vec2( 0.707, -0.707), vec2(-0.707, -0.707));

    vec2 px = (radiusPx * devicePixelRatio) / viewportSize;

    float response = 0.0;
    for (int i = 0; i < 8; ++i) {
        float di = texture(uDepth, uv + dirs[i] * px).r;
        response += max(0.0, logD0 - log2(di + 1e-6));
    }
    response /= 8.0;

    float shade = exp(-response * strength);

    // Premultiplied alpha so the swap-chain composite (One, OneMinusSrcAlpha)
    // leaves the Scene-layer pixels intact wherever the cloud is transparent.
    fragColor = vec4(src.rgb * shade * src.a, src.a);

    // Forward the cloud's depth into the swap-chain depth buffer so that
    // Opaque-pass draws (grid, lines, etc.) that come after the composite
    // can z-test correctly against the cloud's surface. The depth value
    // sampled from uDepth is already in the backend's window-space depth
    // range ([0,1] post-clipSpaceCorrMatrix), so it can be written directly.
    gl_FragDepth = d0;
}
