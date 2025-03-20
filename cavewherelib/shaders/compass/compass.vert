/**************************************************************************
**
**    Copyright (C) 2024 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440

layout(location = 0) in vec3 qt_Vertex;
layout(location = 1) in vec3 qt_Color;

layout(location = 0) out vec3 color;

layout(std140, binding = 0) uniform Uniforms {
    mat4 qt_ModelViewProjectionMatrix;
    // bool ignoreColor;
};

void main(void)
{
    // if (ignoreColor) {
    //     color = vec3(0.0, 0.0, 0.0);
    // } else {
        color = qt_Color;
    // }
    gl_Position = qt_ModelViewProjectionMatrix * vec4(qt_Vertex, 1.0);
}
