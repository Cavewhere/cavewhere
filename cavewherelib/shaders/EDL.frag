/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440 core

// Eye-Dome Lighting (Boucheny 2009 / CloudCompare / Potree), run as the final
// composite. The scene (Background + Opaque) and the point cloud were rendered
// into separate 1x color textures that share one depth buffer, so the cloud is
// already hardware-occluded by scene geometry and the depth buffer holds the
// combined cloud+scene depth. This pass writes the final swap-chain pixel:
// scene where there is no cloud, shaded cloud where there is.
//
// For each cloud pixel we look at a ring of 8 neighbors. Each neighbor darkens
// the center by ONE shared transfer function of the log-depth gap g:
//
//     darkening = min(max(0, g) * strength, maxDarken)
//
// keyed only on which pixel is the near side of the edge:
//  - cloud neighbor closer than the center (g = logZ0 - logZi): far-side shape
//    darkening — the depth cue within a continuous cloud surface and the far lip
//    of a cloud↔cloud cliff.
//  - non-cloud neighbor farther than the center (g = logZi - logZ0): the cloud
//    ends here, so its near edge is outlined against the scene/background.
// Both use the same strength and maxDarken, so a cloud↔cloud edge and a
// cloud↔opaque edge of the same depth gap darken identically — the silhouette
// reads the same regardless of what the cloud borders. strength is the slope
// (depth sensitivity); maxDarken is the ceiling; an edge reaches full dark once
// the gap exceeds maxDarken/strength.
//
// Neighbors are classified cloud vs non-cloud by the cloud texture's alpha
// (cloud is opaque, everything else is transparent).
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
    // Effective slope of the shared darkening transfer function. Computed CPU-side
    // as baseline / max(log2(linearDepthFromNear(1)/linearDepthFromNear(0.5)), 1)
    // so the same baseline gives a matching EDL response in ortho and perspective.
    // An edge reaches maxDarken once the log-depth gap exceeds maxDarken/strength.
    float strength;
    // 1.0 when the backend's framebuffer is Y-down (Metal/Vulkan/D3D) so the
    // offscreen texture's V axis follows native top-left convention and we
    // must flip V when sampling; 0.0 on OpenGL (Y-up framebuffer).
    float textureYFlip;
    // radiusPx * devicePixelRatio / viewportSize, precomputed CPU-side.
    vec2 sampleOffset;
    // Ceiling on per-neighbor darkening — the darkness an edge reaches at a large
    // depth gap, shared by the far-side and border cases.
    float maxDarken;
};

layout(binding = 2) uniform sampler2D uCloudColor;   // point cloud (opaque where present)
layout(binding = 3) uniform sampler2D uDepth;         // combined cloud + scene depth
layout(binding = 4) uniform sampler2D uSceneColor;    // Background + Opaque, drawn at 1x

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

    // Combined cloud+scene depth, already in the backend's window-space range
    // ([0,1] post-clipSpaceCorrMatrix). Forward it to the swap-chain depth
    // buffer so post-composite passes (Transparent, Overlay) z-test correctly
    // against everything drawn so far.
    float d0 = texture(uDepth, uv).r;
    gl_FragDepth = d0;

    vec4 cloud = texture(uCloudColor, uv);

    // No cloud here: pass the scene (Background + Opaque) straight through. This
    // also covers pixels where opaque geometry occluded the cloud (the cloud
    // failed the shared-depth test, so cloud.a stayed at its cleared zero). The
    // scene color is fetched only on this branch — cloud pixels (the 8-tap path
    // below) never read it, so sampling it here keeps that texel fetch off them.
    if (cloud.a < kCloudAlphaThreshold) {
        fragColor = texture(uSceneColor, uv);
        return;
    }

    // Linearize as distance from near plane (always >= 0).
    float logZ0 = log2(linearDepthFromNear(d0) + 1e-6);

    const vec2 dirs[8] = vec2[](
        vec2( 1.0,  0.0), vec2(-1.0,  0.0), vec2( 0.0,  1.0), vec2( 0.0, -1.0),
        vec2( 0.707,  0.707), vec2(-0.707,  0.707),
        vec2( 0.707, -0.707), vec2(-0.707, -0.707));

    float response = 0.0;
    for (int i = 0; i < 8; ++i) {
        vec2 nuv = uv + dirs[i] * sampleOffset;
        float logZi = log2(linearDepthFromNear(texture(uDepth, nuv).r) + 1e-6);
        // Far side for a closer cloud neighbor, near side for a farther non-cloud
        // neighbor — one shared transfer function either way.
        float g = (texture(uCloudColor, nuv).a > kCloudAlphaThreshold)
                  ? (logZ0 - logZi)
                  : (logZi - logZ0);
        response += min(max(0.0, g) * strength, maxDarken);
    }

    // Average over the 8 taps.
    float shade = exp(-response / 8.0);

    // The cloud is opaque where present, so it fully covers the scene here.
    fragColor = vec4(cloud.rgb * shade, 1.0);
}
