/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440 core

// Fullscreen triangle synthesised from gl_VertexIndex — no vertex buffer.
// Verts 0,1,2 produce a single triangle that covers the [-1,1] viewport.
layout(location = 0) out vec2 vUV;

out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    vec2 pos = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    vUV = pos;
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}
