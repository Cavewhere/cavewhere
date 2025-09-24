// unlit.vert (GLSL 450 for Vulkan)
#version 450
layout(location = 0) in vec3 inPosition;
// layout(location = 1) in vec3 inNormal;   // optional
// layout(location = 2) in vec4 inTangent;  // optional
// layout(location = 3) in vec2 inTexCoord0;

layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 viewProj;
} sceneUbo;

layout(set = 0, binding = 1) uniform ModelUBO {
    mat4 model;
} modelUbo;

layout(location = 0) out vec2 v_uv;

void main() {
    // v_uv = inTexCoord0;
    gl_Position = sceneUbo.viewProj * modelUbo.model * vec4(inPosition, 1.0);
}
