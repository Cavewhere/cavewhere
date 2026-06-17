/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// simple vertex shader

#version 440 core

layout(location = 0) in vec3 vVertex;

layout(location = 0) out vec4 vPosition;
layout(location = 1) out vec4 projectedPosition;

// Shared scene camera — same buffer and layout every other render object binds
// at slot 0 (cwRhiScene::GlobalUniform).
layout(std140, binding = 0) uniform Global {
    mat4 viewProjectionMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    float devicePixelRatio;
};

// Grid placement (ModelMatrix) + the scale-only matrix used for contour sampling.
layout(std140, binding = 1) uniform Matrices {
    mat4 ModelMatrix;
    mat4 ScaleMatrix;
};

void main() {
    vec4 position = vec4(vVertex, 1.0);
    gl_Position = viewProjectionMatrix * ModelMatrix * position;
    projectedPosition = gl_Position;
    vPosition.xyz = (ScaleMatrix * position).xyz;
}
