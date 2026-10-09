/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440 core

layout(location = 0) flat in uint vType; // 0 = centerline, non-zero = splay
layout(location = 1) in float vValue;    // world z (depth) or a per-segment value

layout(location = 0) out vec4 fragColor;

// Mirrors cwColorByUniform; keep the two in sync.
const int kRampStops = 32;
layout(std140, binding = 1) uniform ColorByBlock {
    vec4 ramp[kRampStops];
    vec4 range;   // rangeMin, rangeMax, pivot, halfExtent
    vec4 params;  // mode, strength, slotCount, valueSource
};

const float kModeOff = 0.0;
const float kModeSequential = 1.0;
const float kModeDiverging = 2.0;
const float kModeCategorical = 3.0;

const vec4 kCenterlineColor = vec4(1.0, 0.0, 0.0, 1.0);
const vec4 kSplayColor = vec4(1.0, 0.6, 0.6, 1.0);
// Splays keep reading as secondary under color-by: their ramp color is pulled
// this far toward white.
const float kSplayLighten = 0.5;

float normalizeValue(float v)
{
    float mode = params.x;
    if (mode == kModeDiverging) {
        return clamp(0.5 + 0.5 * (v - range.z) / range.w, 0.0, 1.0);
    }
    return clamp((v - range.x) / (range.y - range.x), 0.0, 1.0);
}

vec4 rampColor(float t)
{
    float x = clamp(t, 0.0, 1.0) * float(kRampStops - 1);
    int i = int(floor(x));
    int j = min(i + 1, kRampStops - 1);
    return mix(ramp[i], ramp[j], x - float(i));
}

vec4 slotColor(float slot)
{
    int i = clamp(int(slot + 0.5), 0, kRampStops - 1);
    return ramp[i];
}

void main(void)
{
    float mode = params.x;
    bool isSplay = vType != 0u;
    if (mode == kModeOff) {
        fragColor = isSplay ? kSplayColor : kCenterlineColor;
        return;
    }

    vec4 color = mode == kModeCategorical
        ? slotColor(vValue)
        : rampColor(normalizeValue(vValue));
    if (isSplay) {
        color.rgb = mix(color.rgb, vec3(1.0), kSplayLighten);
    }
    fragColor = vec4(color.rgb, 1.0);
}
