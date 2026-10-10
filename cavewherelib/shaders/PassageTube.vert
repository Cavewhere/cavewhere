/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440 core

layout(location = 0) in vec3 position;

layout(location = 0) out vec3 vViewPosition;

layout(std140, binding = 0) uniform GlobalBlock {
    mat4 viewProjectionMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    float devicePixelRatio;
    vec2 viewportSize; // pass render target size in device pixels
};

void main(void)
{
    vec4 worldPosition = vec4(position, 1.0);
    vViewPosition = (viewMatrix * worldPosition).xyz;
    gl_Position = viewProjectionMatrix * worldPosition;
}
