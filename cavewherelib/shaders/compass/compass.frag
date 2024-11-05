/**************************************************************************
**
**    Copyright (C) 2024 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440

layout(location = 0) in vec3 color;
layout(location = 0) out vec4 fragColor;

void main(void)
{
    fragColor = vec4(color, 1.0);
}
