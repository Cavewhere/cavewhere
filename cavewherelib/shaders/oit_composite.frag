#version 440 core

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D AccumTexture;

const float EPSILON = 1e-5;

void main()
{
    vec4 accum = texture(AccumTexture, vTexCoord);
    float accumAlpha = accum.a;
    if (accumAlpha <= EPSILON) {
        fragColor = vec4(0.0);
        return;
    }

    float transparency = 1.0 - exp(-accumAlpha);
    vec3 avgColor = accum.rgb / accumAlpha;
    fragColor = vec4(avgColor * transparency, transparency);
}
