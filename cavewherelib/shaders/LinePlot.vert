/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440 core

layout(location = 0) in vec3 vVertex;
// layout(location = 0) out float depth;

layout(std140, binding = 0) uniform GlobalBlock {
    mat4 viewProjectionMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    float devicePixelRatio;
};

// layout(std140, binding = 1) uniform UniformBlock
// {
//     float MaxZValue;
//     float MinZValue;
// };

void main(void)
{
    //depth = (vVertex.z - MinZValue) / (MaxZValue - MinZValue); // Normalize the z value
    gl_Position = viewProjectionMatrix * vec4(vVertex, 1.0);
    gl_Position.z -= 1e-4;
}
