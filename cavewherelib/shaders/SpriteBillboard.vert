/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440 core

// One instance per sprite billboard, expanded to a camera-facing quad: the
// pipeline draws a fixed 4-vertex TriangleStrip and the corner is selected from
// gl_VertexIndex, so there is no per-vertex buffer at all. A port of
// cw::billboard::buildModelMatrix with the offsets applied in view space, where
// the camera's right and up axes are +x and +y, so no matrix inverse is needed.
layout(location = 0) in vec3 iWorldPosition;
// World units toward the viewer (cwBillboardSightLine), so a marker sitting on
// a wall is in front of it.
layout(location = 1) in float iDepthBias;
layout(location = 2) in float iOpacity;

layout(location = 0) out vec2 vUv;
layout(location = 1) out float vOpacity;

layout(std140, binding = 0) uniform GlobalBlock {
    mat4 viewProjectionMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    float devicePixelRatio;
    vec2 viewportSize; // pass render target size in device pixels
};

// Mirrors SpriteBlock in cwRenderSpriteBillboards.cpp.
layout(std140, binding = 1) uniform SpriteBlock {
    vec2 spriteSizePx;   // the prototype's size in logical pixels
    float sizeMode;      // kSizeModeWorldSized or screen constant
    float pad;
};

const float kSizeModeWorldSized = 0.0;
// World units per content pixel for WorldSized sprites; mirrors
// cwBillboardGeometry.cpp.
const float kWorldPerPixel = 0.02;
// Element (3, 2) in Qt's row, column terms, projectionMatrix[2][3] here: 0 for
// an orthographic projection and -1 for a perspective one; this midpoint
// classifies a blended transition frame. Mirrors isOrthographic().
const float kPerspectiveDepthDivide = -0.5;
// Floors guarding the screen-constant size division, as in cwBillboardGeometry.
const float kMinClipW = 1e-4;
const float kMinProjectionScale = 1e-6;

void main(void)
{
    // gl_VertexIndex 0..3 -> (0,0) (1,0) (0,1) (1,1), also the uv: the texture
    // has an Image's orientation, so (0,0) is the content's top-left corner.
    vec2 corner = vec2(float(gl_VertexIndex & 1), float(gl_VertexIndex >> 1));
    vec2 offsetPx = (corner - 0.5) * spriteSizePx;
    vUv = corner;
    vOpacity = iOpacity;

    vec4 viewPos = viewMatrix * vec4(iWorldPosition, 1.0);
    bool orthographic = projectionMatrix[2][3] >= kPerspectiveDepthDivide;
    // Toward the viewer: +z in view space under a parallel projection, toward the
    // eye (the view-space origin) under perspective.
    vec3 towardViewer = orthographic ? vec3(0.0, 0.0, 1.0) : normalize(-viewPos.xyz);
    viewPos.xyz += towardViewer * iDepthBias;

    vec2 worldPerPixel;
    if (sizeMode == kSizeModeWorldSized) {
        worldPerPixel = vec2(kWorldPerPixel);
    } else {
        // Invert the projection so one content pixel covers one logical screen
        // pixel at the unbiased depth, so the bias changes depth and never size.
        float w = max(abs((viewProjectionMatrix * vec4(iWorldPosition, 1.0)).w), kMinClipW);
        vec2 logicalViewport = viewportSize / devicePixelRatio;
        vec2 projectionScale = max(abs(vec2(projectionMatrix[0][0], projectionMatrix[1][1])),
                                   vec2(kMinProjectionScale));
        worldPerPixel = 2.0 * w / (projectionScale * logicalViewport);
    }

    // Content pixel space is y-down, view space is y-up.
    viewPos.xy += vec2(offsetPx.x, -offsetPx.y) * worldPerPixel;
    gl_Position = projectionMatrix * viewPos;
}
