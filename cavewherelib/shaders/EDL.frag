/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440 core

// Eye-Dome Lighting (Boucheny 2009 / CloudCompare / Potree). For each pixel,
// compare its depth to a ring of 8 neighbors. Pixels behind a silhouette
// edge (cloud neighbors closer) get darkened in proportion to the depth gap.
//
// On top of that classic far-side shading we add a near-side silhouette term:
// where a cloud edge borders a non-cloud neighbor (background here; opaque
// scene geometry once the shared-depth pass lands) the cloud edge itself is
// darkened, so the cloud keeps a visible outline instead of bleeding into the
// background. Neighbors are classified cloud vs non-cloud by the color
// texture's alpha (cloud is opaque, background is transparent).
//
// Operates in log2(linear view-space Z). The window-space depth buffer is
// hyperbolic under perspective (d = A + B/z_view), so log2 of the raw
// depth would produce wildly different EDL intensities at different
// distances from the camera. Linearizing to view-space Z first makes the
// response depend on the depth-ratio between neighbors, not on where you
// happen to be in the depth range. Works the same way under orthographic,
// where d is already linear in z_view.

layout(std140, binding = 0) uniform GlobalBlock {
    mat4 viewProjectionMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    float devicePixelRatio;
    vec2 viewportSize;
};

layout(std140, binding = 1) uniform EdlBlock {
    // Effective darkening exponent. Computed CPU-side as
    //   baseline_strength / max(log2(linearDepthFromNear(1)/linearDepthFromNear(0.5)), 1)
    // so silhouette response stays consistent across ortho and perspective.
    float strength;
    // 1.0 when the backend's framebuffer is Y-down (Metal/Vulkan/D3D) so the
    // offscreen texture's V axis follows native top-left convention and we
    // must flip V when sampling; 0.0 on OpenGL (Y-up framebuffer).
    float textureYFlip;
    // radiusPx * devicePixelRatio / viewportSize, precomputed CPU-side.
    vec2 sampleOffset;
    // Near-side silhouette darkening for cloud edges that border non-cloud
    // pixels. silhouetteClamp caps each neighbor's log-depth gap so a far
    // background neighbor saturates instead of driving the edge to pure black;
    // silhouetteStrength scales the (clamped, averaged) result.
    float silhouetteStrength;
    float silhouetteClamp;
};

layout(binding = 2) uniform sampler2D uColor;
layout(binding = 3) uniform sampler2D uDepth;

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 fragColor;

// Cloud-vs-background cutoff on the color texture's alpha: cloud pixels are
// opaque, background/non-cloud is transparent. Shared by the center-pixel
// discard and the per-neighbor mask in the EDL loop so they can't drift apart.
const float kCloudAlphaThreshold = 0.001;

// Linear distance from the near plane to the fragment, in view-space units.
// Always >= 0 (0 at near, far-near at far). Used instead of abs(view-space Z)
// because CaveWhere's ortho camera uses a symmetric near=-far setup — the
// visible range straddles z_view = 0, and log2(abs(z_view)) would hit -inf
// at the camera plane, blowing up the EDL response to fully black there.
// Distance-from-near is always positive and bounded away from zero except
// exactly at the near plane (clamped with +1e-6 at sample time).
//
// projectionMatrix here is Qt RHI's clipSpaceCorrMatrix * P, so window-space
// depth is [0,1]. P[2][3] (column 2, row 3) is -1 for perspective, 0 for ortho.
float linearDepthFromNear(float d)
{
    float a = projectionMatrix[2][2];
    float b = projectionMatrix[3][2];
    if (projectionMatrix[2][3] < -0.5) {
        // Perspective. Derivation:
        //   z_view(d)  = -b / (d + a)
        //   z_near     = z_view(0) = -b/a
        //   depth_from_near = z_near - z_view(d)
        //                   = -b/a + b/(d+a)
        //                   = -b * d / ((d + a) * a)
        return -b * d / ((d + a) * a);
    }
    // Orthographic. d = a*z + b, so z_view(d) = (d-b)/a and z_near = -b/a.
    // depth_from_near = z_near - z_view(d) = -d/a.
    return -d / a;
}

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
    if (src.a < kCloudAlphaThreshold) {
        discard;
    }

    // Linearize as distance from near plane (always >= 0).
    float logZ0 = log2(linearDepthFromNear(d0) + 1e-6);

    const vec2 dirs[8] = vec2[](
        vec2( 1.0,  0.0), vec2(-1.0,  0.0), vec2( 0.0,  1.0), vec2( 0.0, -1.0),
        vec2( 0.707,  0.707), vec2(-0.707,  0.707),
        vec2( 0.707, -0.707), vec2(-0.707, -0.707));

    // Classify each neighbor with the cloud mask (the color texture's alpha).
    //  - cloud neighbor closer than the center -> far-side shape darkening,
    //    the classic EDL look that gives interior depth cues.
    //  - non-cloud neighbor (farther by construction; occluded cloud is
    //    rejected before this pass) -> near-side silhouette darkening so the
    //    cloud edge keeps an outline against the background.
    float shapeResponse = 0.0;
    float silhouetteResponse = 0.0;
    for (int i = 0; i < 8; ++i) {
        vec2 nuv = uv + dirs[i] * sampleOffset;
        float logZi = log2(linearDepthFromNear(texture(uDepth, nuv).r) + 1e-6);
        if (texture(uColor, nuv).a > kCloudAlphaThreshold) {
            shapeResponse += max(0.0, logZ0 - logZi);
        } else {
            silhouetteResponse += clamp(logZi - logZ0, 0.0, silhouetteClamp);
        }
    }

    // Average over the 8 taps (the single /8 covers both terms).
    float shade = exp(-(shapeResponse * strength
                        + silhouetteResponse * silhouetteStrength) / 8.0);

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
