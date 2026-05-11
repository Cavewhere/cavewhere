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

// TODO: promote to a per-layer uniform once UI sliders for point size land.
const float basePointSize = 0.05;
const float maxPointSizePx = 16.0;

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
};

void main(void)
{
    gl_Position = viewProjectionMatrix * vec4(vVertex, 1.0);

    // Perspective-correct screen size of a fixed world-space radius: a sphere of
    // radius `basePointSize` at clip-space depth `gl_Position.w` projects to
    // `projectionMatrix[1][1] * viewportSize.y * 0.5 / gl_Position.w` pixels.
    // abs() because projectionMatrix is pre-multiplied by Qt RHI's
    // clipSpaceCorrMatrix — [1][1] is negative on Metal/Vulkan/D3D and a
    // signed sizePx would always hit the lower clamp on those backends.
    float sizePx = basePointSize * devicePixelRatio
                 * abs(projectionMatrix[1][1]) * viewportSize.y * 0.5
                 / gl_Position.w;
    // 5px floor so far-away points stay visible (1px sprites alias to noise)
    // and so each sprite covers more than EDL's sampling radius — otherwise
    // the silhouette response samples background and no rim shading appears.
    gl_PointSize = clamp(sizePx, 5.0, maxPointSizePx);
}
