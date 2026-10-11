/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440 core

layout(location = 0) in vec3 vViewPosition;
layout(location = 1) in vec3 vWorldPosition;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 1) uniform TubeBlock {
    vec4 color;
    float ambient;
    float yUp; // 1 when the framebuffer y axis points up, -1 when it points down
    vec4 clipPlane;
    float clipEnabled; // 1 discards fragments on the clip plane's positive side
};

const vec3 kHeadlight = vec3(0.0, 0.0, 1.0);
const float kMinNormalLength = 1e-12;
const float kWireframeShade = 0.4;

void main(void)
{
#ifdef WIREFRAME
    fragColor = vec4(color.rgb * kWireframeShade, color.a);
#else
    // A screen-derivative normal of a visible fragment faces the viewer in both
    // perspective and orthographic projections; only the framebuffer y
    // direction flips its sign. Walls then light the same from inside and outside.
    vec3 normal = cross(dFdx(vViewPosition), dFdy(vViewPosition)) * yUp;
    normal = length(normal) > kMinNormalLength ? normalize(normal) : kHeadlight;

    float diffuse = max(dot(normal, kHeadlight), 0.0);
    fragColor = vec4(color.rgb * (ambient + (1.0 - ambient) * diffuse), color.a);
#endif

    // Discarded last, because the derivatives above need every fragment of the quad.
    if (clipEnabled > 0.5 && dot(vec4(vWorldPosition, 1.0), clipPlane) > 0.0) {
        discard;
    }
}
