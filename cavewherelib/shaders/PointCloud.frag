/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440 core

// Flat white unless a debug view colors the point (PointCloud.vert)
layout(location = 0) in vec4 vColor;

layout(location = 0) out vec4 fragColor;

void main(void)
{
    fragColor = vColor;
}
