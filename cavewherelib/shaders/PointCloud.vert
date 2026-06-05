/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440 core

layout(location = 0) in vec3 vVertex;

layout(std140, binding = 0) uniform GlobalBlock {
    mat4 viewProjectionMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    float devicePixelRatio;
    vec2 viewportSize;
};

// Per-cloud (binding 1): world-space sprite radius in meters. Uploaded by
// cwRHIPointCloud whenever cwLazLayersSceneNode::setWorldRadius runs (hold
// P + mouse wheel in the 3D view tunes it interactively; sink_repatcher
// --point-radius sets it for offline renders). std140 rounds the block to
// 16 bytes; the C++ PerCloudUniform pads to match.
layout(std140, binding = 1) uniform PerCloudBlock {
    float worldRadius;
};

const float maxPointSizePx = 64.0;

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
};

void main(void)
{
    gl_Position = viewProjectionMatrix * vec4(vVertex, 1.0);

    // Perspective-correct screen size of a fixed world-space radius: a sphere
    // of radius `worldRadius` at clip-space depth `gl_Position.w` projects to
    // `projectionMatrix[1][1] * viewportSize.y * 0.5 / gl_Position.w` pixels.
    // abs() because projectionMatrix is pre-multiplied by Qt RHI's
    // clipSpaceCorrMatrix — [1][1] is negative on Metal/Vulkan/D3D and a
    // signed sizePx would always hit the lower clamp on those backends.
    float sizePx = worldRadius * devicePixelRatio
                 * abs(projectionMatrix[1][1]) * viewportSize.y * 0.5
                 / gl_Position.w;
    // 1px floor: zoomed-out clouds were getting darker than zoomed-in ones
    // because a large min clamp made far points overdraw and stack EDL
    // darkening. Letting size shrink to a single pixel keeps overdraw flat
    // across zoom levels. EDL response gets noisier on lone 1px sprites but
    // that's preferable to the inconsistent intensity.
    gl_PointSize = clamp(sizePx, 1.0, maxPointSizePx);
}
