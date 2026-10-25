/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440 core

layout(location = 0) in vec2 vUv;
layout(location = 1) in float vOpacity;

layout(location = 0) out vec4 fragColor;

// The prototype's layer texture, premultiplied.
layout(binding = 2) uniform sampler2D sprite;

// Fragments below this alpha are discarded so they write no depth: the depth
// the pass leaves behind follows the marker's silhouette, not its quad. Low
// enough that the anti-aliased rim keeps blending.
const float kAlphaCutoff = 0.05;

void main(void)
{
    vec4 color = texture(sprite, vUv);
    if (color.a < kAlphaCutoff) {
        discard;
    }
    // Opacity scales all four premultiplied channels.
    fragColor = color * vOpacity;
}
