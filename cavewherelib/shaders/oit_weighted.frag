#version 440 core

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 accumOutput;

layout(binding = 1) uniform sampler2D Texture;

float computeWeight(float alpha, float depth)
{
    const float epsilon = 0.01;
    float depthFactor = clamp(1.0 - depth, 0.0, 1.0);
    return max(alpha * depthFactor + epsilon, epsilon);
}

void main()
{
    vec4 color = texture(Texture, vTexCoord);
    float alpha = clamp(color.a, 0.0, 1.0);
    if (alpha <= 0.0) {
        accumOutput = vec4(0.0);
        return;
    }

    float weight = computeWeight(alpha, gl_FragCoord.z);
    vec3 weightedColor = color.rgb * alpha * weight;
    float weightedAlpha = alpha * weight;
    accumOutput = vec4(weightedColor, weightedAlpha);
}
