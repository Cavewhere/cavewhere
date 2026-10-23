/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440 core

layout(location = 0) in vec4 vColor;
layout(location = 1) in float vDash;

layout(location = 0) out vec4 fragColor;

const float kGapStart = 0.5;

void main(void)
{
    // A solid edge carries a negative dash coordinate
    if (vDash >= 0.0 && fract(vDash) >= kGapStart) {
        discard;
    }
    fragColor = vColor;
}
