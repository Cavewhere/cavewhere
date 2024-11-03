#version 440

layout(std140, binding = 0) uniform UniformData {
    vec4 color1;
    vec4 color2;
    vec2 center;
    float radius;
    float radiusOffset;
};

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

void main()
{
    float dist = distance(v_uv, center);
    float t = smoothstep(radius, radius - radiusOffset, dist);
    fragColor = mix(color1, color2, t);
}
