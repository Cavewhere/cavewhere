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
// draws as a single instance, so every point of it dequantizes with these.
layout(location = 1) in vec4 nodeOriginScale;

layout(std140, binding = 0) uniform GlobalBlock {
    mat4 viewProjectionMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    float devicePixelRatio;
    vec2 viewportSize;
};

// Per-cloud (binding 1): what every point of this cloud sizes itself from.
// spacingCoverage is the only knob a user turns (hold P + mouse wheel in the
// 3D view): the fraction of its own cell a sprite covers. sseThresholdPx is
// the projected spacing the octree cut refines to — the view's
// screenSpaceErrorPx times this cloud's current inflation — which is what lets
// each vertex work out the spacing around it from its own depth. drawnSpacing
// is the finest sample spacing the last live frame actually drew, in meters,
// which is what the sprites have to close when the cut is coarser than the
// threshold asked for. Four floats is std140's 16 bytes exactly; the C++
// PerCloudUniform matches it field for field, padding included.
layout(std140, binding = 1) uniform PerCloudBlock {
    float spacingCoverage;
    float sseThresholdPx;
    float drawnSpacing;
    float padding;
};

const float maxPointSizePx = 64.0;

// Keeps the sizing finite for points that straddle or sit behind the eye,
// matching cw::sse::kMinimumClipW so the shader and node selection floor w at
// the same place.
const float minimumClipW = 1e-4;

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
};

void main(void)
{
    vec3 vVertex = nodeOriginScale.xyz + vec3(qpos.xyz) * nodeOriginScale.w;

    gl_Position = viewProjectionMatrix * vec4(vVertex, 1.0);

    // Perspective-correct screen size of a world-space length: a length of L
    // meters at clip-space depth w covers
    // L * projectionMatrix[1][1] * viewportSize.y * 0.5 / w pixels.
    // viewportSize is already the target's physical pixel size, so
    // devicePixelRatio stays out of this — a second factor drew sprites twice
    // as wide on a 2x display as selection (cw::sse::pixelsPerMeter) assumed.
    // abs() because projectionMatrix is pre-multiplied by Qt RHI's
    // clipSpaceCorrMatrix — [1][1] is negative on Metal/Vulkan/D3D and a
    // signed sizePx would always hit the lower clamp on those backends.
    float pixelsPerMeter = abs(projectionMatrix[1][1]) * viewportSize.y * 0.5
                         / max(gl_Position.w, minimumClipW);

    // Sized against the spacing the cut is aiming for right here: the octree
    // refines a node while its sample spacing projects wider than
    // sseThresholdPx, so the points around this vertex sit about
    // sseThresholdPx / pixelsPerMeter meters apart — finer close to the eye,
    // coarser far from it, exactly how the drawn level steps out with distance
    // in perspective. Covering spacingCoverage of that gap is therefore a
    // constant pixel size, which is what lets a far tile drawn at a coarse
    // level cover its own cell as well as a near tile covers its finer one.
    // Coverage is the whole rule: it is the one number that predicts whether
    // the surface reads solid or shows holes, so there is no separate tuned
    // world radius competing with it.
    //
    // The threshold term describes the spacing a cut that keeps up has on
    // screen. Where the point budget or a node still streaming holds the cloud
    // coarser than that, drawnSpacing is the wider gap that is really there,
    // so coverage of it becomes a world-space floor — one number for the whole
    // cloud, because the cut is additive and a per-node floor would blow a
    // refined region's ancestors up to their own coarse spacing.
    float sizePx = max(spacingCoverage * drawnSpacing * pixelsPerMeter,
                       spacingCoverage * sseThresholdPx);
    // 1px floor: zoomed-out clouds were getting darker than zoomed-in ones
    // because a large min clamp made far points overdraw and stack EDL
    // darkening. Letting size shrink to a single pixel keeps overdraw flat
    // across zoom levels. EDL response gets noisier on lone 1px sprites but
    // that's preferable to the inconsistent intensity.
    gl_PointSize = clamp(sizePx, 1.0, maxPointSizePx);
}
