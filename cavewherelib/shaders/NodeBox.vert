/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440 core

// One corner of a unit cube's edge list (cwRHIPointCloud's node boxes): xyz in
// [0, 1], and w is 0 at the edge's first end and 1 at its second, which is
// what the dashes are measured along.
layout(location = 0) in vec4 corner;

// Per instance, one per box: the node cube's min corner and side, then its
// color with a = 1 for a dashed box (a node still loading), 0 for a solid one.
layout(location = 1) in vec4 boxOriginSize;
layout(location = 2) in vec4 boxColor;

layout(std140, binding = 0) uniform GlobalBlock {
    mat4 viewProjectionMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    float devicePixelRatio;
    vec2 viewportSize;
};

layout(location = 0) out vec4 vColor;
layout(location = 1) out float vDash;

const float kDashesPerEdge = 8.0;
const float kDashedThreshold = 0.5;
const float kSolid = -1.0;

void main(void)
{
    vec3 position = boxOriginSize.xyz + corner.xyz * boxOriginSize.w;
    gl_Position = viewProjectionMatrix * vec4(position, 1.0);
    vColor = vec4(boxColor.rgb, 1.0);
    vDash = boxColor.a > kDashedThreshold ? corner.w * kDashesPerEdge : kSolid;
}
