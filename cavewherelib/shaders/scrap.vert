#version 440 core

layout(location = 0) in vec3 vVertex;
layout(location = 1) in vec2 vScrapTexCoords;

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec3 vPosition;

layout(std140, binding = 0) uniform GlobalBlock {
    mat4 viewProjectionMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    float devicePixelRatio;
};

// layout(std140, binding = 1) uniform ScrapBlock {
//     vec2 vTexCoordsScale;
// };

void main() {
    vPosition = vVertex;
    gl_Position = viewProjectionMatrix * vec4(vVertex, 1.0);
    vTexCoord = vScrapTexCoords;

}
