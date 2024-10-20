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

layout(std140, binding = 0) uniform Matrices {
    mat4 ModelViewProjectionMatrix;
    mat4 ModelMatrix;
};

const float z = 0.0;

void main() {
    vec4 position = vec4(vVertex, 1.0);
    gl_Position = ModelViewProjectionMatrix * position;
    projectedPosition = gl_Position;
    vPosition.xyz = (ModelMatrix * position).xyz;
    // vPosition.xyz = (position).xyz;
}
