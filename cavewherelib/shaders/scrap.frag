#version 440 core

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec3 vWorldPos;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D Texture;

// Per-draw block: the textured-items payload prefix (model matrix, unused by
// scraps), the shared cwColorByUniform, then the per-item tail. The middle
// mirrors cwColorByUniform; keep the two in sync.
const int kRampStops = 32;
layout(std140, binding = 2) uniform ScrapBlock {
    mat4 modelMatrix;
    vec4 ramp[kRampStops];
    vec4 range;   // rangeMin, rangeMax, pivot, halfExtent
    vec4 params;  // mode, strength, slotCount, valueSource
    vec4 item;    // x = per-item color-by value
};

const float kModeOff = 0.0;
const float kModeDiverging = 2.0;
const float kModeCategorical = 3.0;
const float kValueSourcePerElement = 1.0;
// Rec. 601 luma; the paper's brightness survives the recolor so ink stays dark.
const vec3 kLumaWeights = vec3(0.299, 0.587, 0.114);

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

void main() {
    vec4 textureSample = texture(Texture, vTexCoord);
    // Make fully transparent texels show as white instead of black.
    vec3 paper = mix(vec3(1.0), textureSample.rgb, textureSample.a);

    if (params.x != kModeOff) {
        float value = params.w == kValueSourcePerElement ? item.x : vWorldPos.z;
        vec4 color = params.x == kModeCategorical
            ? slotColor(value)
            : rampColor(normalizeValue(value));
        float luma = dot(paper, kLumaWeights);
        paper = mix(paper, color.rgb * luma, params.y);
    }

    fragColor = vec4(paper, textureSample.a);
}
