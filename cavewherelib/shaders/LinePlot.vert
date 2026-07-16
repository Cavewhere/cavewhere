/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440 core

layout(location = 0) in vec3 vVertex;
// Per-vertex visibility: 0 = hidden, non-zero = visible. Both vertices of a
// shot carry the same value, so a hidden shot collapses both its endpoints
// outside the clip volume and the line is discarded.
layout(location = 1) in uint vVisibility;
// layout(location = 0) out float depth;

layout(std140, binding = 0) uniform GlobalBlock {
    mat4 viewProjectionMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    float devicePixelRatio;
};

void main(void)
{
    if (vVisibility == 0u) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0); // outside [-w,w] clip volume -> discarded
        return;
    }

    //depth = (vVertex.z - MinZValue) / (MaxZValue - MinZValue); // Normalize the z value
    gl_Position = viewProjectionMatrix * vec4(vVertex, 1.0);
    gl_Position.z -= 1e-4;
}
