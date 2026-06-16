/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440 core

layout(location = 0) in vec3 vVertex;
layout(location = 1) in uint vTripId;
// layout(location = 0) out float depth;

layout(std140, binding = 0) uniform GlobalBlock {
    mat4 viewProjectionMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    float devicePixelRatio;
};

// Per-trip visibility lookup: one texel per running trip id, R8 in [0,1]
// (1.0 = visible, 0.0 = hidden). texelFetch ignores filtering, so this is an
// exact per-trip read. A hidden trip collapses both endpoints of its segments
// outside the clip volume, so the line is discarded.
layout(binding = 1) uniform sampler2D tripVisibility;

void main(void)
{
    float visible = texelFetch(tripVisibility, ivec2(int(vTripId), 0), 0).r;
    if (visible < 0.5) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0); // outside [-w,w] clip volume -> discarded
        return;
    }

    //depth = (vVertex.z - MinZValue) / (MaxZValue - MinZValue); // Normalize the z value
    gl_Position = viewProjectionMatrix * vec4(vVertex, 1.0);
    gl_Position.z -= 1e-4;
}
