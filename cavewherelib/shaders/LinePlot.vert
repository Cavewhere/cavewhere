/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440 core

// One instance per segment, expanded to a screen-space quad: the pipeline
// draws a fixed 4-vertex TriangleStrip and the corner is selected from
// gl_VertexIndex, so there is no per-vertex buffer at all. Metal (and the
// D3D/Vulkan cores) clamp native line width to 1 px; extruding in NDC is the
// standard way to get thicker lines there.
layout(location = 0) in vec3 iFrom;
layout(location = 1) in vec3 iTo;
// 0 = centerline, non-zero = splay. Selects width, color, and depth bias.
layout(location = 2) in uint iType;
// 0 = hidden, non-zero = visible. A hidden segment collapses outside the
// clip volume and the whole quad is discarded.
layout(location = 3) in uint iVisibility;

layout(location = 0) flat out uint vType;

layout(std140, binding = 0) uniform GlobalBlock {
    mat4 viewProjectionMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    float devicePixelRatio;
    vec2 viewportSize; // pass render target size in device pixels
};

const float kCenterlineHalfWidthPx = 1.0; // logical px -> 2 px wide line
const float kSplayHalfWidthPx = 0.5;      // logical px -> 1 px wide line
// Clip-space z pull toward the eye so lines win depth ties against the
// surfaces they run along; splays get a hair less so the centerline wins
// z-fights at shared stations.
const float kCenterlineDepthBias = 1e-4;
const float kSplayDepthBias = 0.5e-4;

void main(void)
{
    if (iVisibility == 0u) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0); // outside [-w,w] clip volume -> discarded
        vType = 0u;
        return;
    }

    vec4 clipFrom = viewProjectionMatrix * vec4(iFrom, 1.0);
    vec4 clipTo   = viewProjectionMatrix * vec4(iTo, 1.0);

    // Screen-space direction of the segment; the perpendicular carries the
    // width. A segment projecting to a point (looking straight down it) has
    // no direction — any one keeps the quad on screen at the right size.
    vec2 ndcFrom = clipFrom.xy / clipFrom.w;
    vec2 ndcTo   = clipTo.xy / clipTo.w;
    vec2 screenVec = (ndcTo - ndcFrom) * viewportSize;
    float screenLength = length(screenVec);
    vec2 dir = screenLength > 1e-6 ? screenVec / screenLength : vec2(1.0, 0.0);
    vec2 sideDir = vec2(-dir.y, dir.x);

    // gl_VertexIndex 0..3 -> (from,-1) (from,+1) (to,-1) (to,+1)
    float endSelect = float(gl_VertexIndex >> 1);
    float side = float(int(gl_VertexIndex & 1) * 2 - 1);

    bool isSplay = iType != 0u;
    float halfWidthPx = (isSplay ? kSplayHalfWidthPx : kCenterlineHalfWidthPx)
        * devicePixelRatio;

    // Extrude sideways for width plus along the segment for a square cap, so
    // quads meeting at a shared station overlap instead of leaving pinholes.
    vec2 offsetPx = (sideDir * side + dir * (endSelect * 2.0 - 1.0)) * halfWidthPx;

    // Offset in NDC, multiplied back by w so clipping and depth interpolation
    // stay correct.
    vec4 clip = mix(clipFrom, clipTo, endSelect);
    clip.xy += offsetPx * (2.0 / viewportSize) * clip.w;
    clip.z -= isSplay ? kSplayDepthBias : kCenterlineDepthBias;

    gl_Position = clip;
    vType = iType;
}
