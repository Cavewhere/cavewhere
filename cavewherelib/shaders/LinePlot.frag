/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440 core

layout(location = 0) flat in uint vType; // 0 = centerline, non-zero = splay

layout(location = 0) out vec4 fragColor;

const vec4 kCenterlineColor = vec4(1.0, 0.0, 0.0, 1.0);
const vec4 kSplayColor = vec4(1.0, 0.6, 0.6, 1.0);

void main(void)
{
    fragColor = vType != 0u ? kSplayColor : kCenterlineColor;
}
