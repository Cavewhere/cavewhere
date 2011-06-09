// simple vertex shader

#version 330

in vec2 vVertex;

uniform mat4 ModelViewProjectionMatrix;

const float z = 0.0;

void main() {
  gl_Position = ModelViewProjectionMatrix * vec4(vVertex, z, 1.0);
}

