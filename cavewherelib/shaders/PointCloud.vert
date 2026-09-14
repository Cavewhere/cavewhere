/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440 core

// One octree node's points, quantized to uint16 per axis inside the node's
// cube (cw::octree::quantize). w is reserved.
layout(location = 0) in uvec4 qpos;

// Per instance, one per node: the node cube's min corner and the world size of
// one quantization step (nodeSize / cw::octree::kQuantMax). The whole node
// draws as a single instance, so every point of it dequantizes with these. .w
// also carries the node's sample spacing: it is .w times kSpacingPerQuantStep.
layout(location = 1) in vec4 nodeOriginScale;

layout(std140, binding = 0) uniform GlobalBlock {
    mat4 viewProjectionMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    float devicePixelRatio;
    vec2 viewportSize;
};

// Per-cloud (binding 1): the floor sprite radius in meters, and the sprite
// radius as a fraction of the drawn node's sample spacing. Uploaded by
// cwRHIPointCloud whenever cwLazLayersSceneNode::setWorldRadius or
// setSpacingCoverage runs (hold P + mouse wheel in the 3D view tunes the radius
// interactively; sink_repatcher --point-radius sets it for offline renders).
// std140 rounds the block to 16 bytes; the C++ PerCloudUniform pads to match.
layout(std140, binding = 1) uniform PerCloudBlock {
    float worldRadius;
    float spacingCoverage;
};

const float maxPointSizePx = 64.0;

// kQuantMax / kSampleGridResolution: turns nodeOriginScale.w, which is
// nodeSize / kQuantMax, into the node's sample spacing.
const float kSpacingPerQuantStep = 65535.0 / 128.0;

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
};

void main(void)
{
    vec3 vVertex = nodeOriginScale.xyz + vec3(qpos.xyz) * nodeOriginScale.w;

    gl_Position = viewProjectionMatrix * vec4(vVertex, 1.0);

    // Grid sampling leaves at most one point per occupied cell of side
    // `nodeSpacing`, so sizing a sprite off that spacing keeps a coarse cut
    // nearly solid. max() keeps the tuned radius wherever it is already
    // larger, which is everywhere the cut is refined.
    float nodeSpacing = nodeOriginScale.w * kSpacingPerQuantStep;
    float radiusWorld = max(worldRadius, spacingCoverage * nodeSpacing);

    // Perspective-correct screen size of a fixed world-space radius: a sphere
    // of radius `radiusWorld` at clip-space depth `gl_Position.w` projects to
    // `projectionMatrix[1][1] * viewportSize.y * 0.5 / gl_Position.w` pixels.
    // viewportSize is already the target's physical pixel size, so
    // devicePixelRatio stays out of this — a second factor drew sprites twice
    // as wide on a 2x display as selection (cw::sse::pixelsPerMeter) assumed.
    // abs() because projectionMatrix is pre-multiplied by Qt RHI's
    // clipSpaceCorrMatrix — [1][1] is negative on Metal/Vulkan/D3D and a
    // signed sizePx would always hit the lower clamp on those backends.
    float sizePx = radiusWorld
                 * abs(projectionMatrix[1][1]) * viewportSize.y * 0.5
                 / gl_Position.w;
    // 1px floor: zoomed-out clouds were getting darker than zoomed-in ones
    // because a large min clamp made far points overdraw and stack EDL
    // darkening. Letting size shrink to a single pixel keeps overdraw flat
    // across zoom levels. EDL response gets noisier on lone 1px sprites but
    // that's preferable to the inconsistent intensity.
    gl_PointSize = clamp(sizePx, 1.0, maxPointSizePx);
}
