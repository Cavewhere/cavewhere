// unlit.frag (GLSL 450 for Vulkan)
#version 450
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D Texture;
// layout(set = 1, binding = 0) uniform sampler2D u_baseColor;   // sampler is okay outside a block

// layout(set = 1, binding = 1) uniform MaterialUBO {
//     vec4 baseColorFactor;
// } materialUbo;

void main() {
    vec4 texC = texture(Texture, v_uv);
    outColor = texC; //* materialUbo.baseColorFactor;
    // outColor = vec4(1.0, 0.0, 0.0, 1.0);
}
