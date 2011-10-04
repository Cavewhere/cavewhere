// simple vertex shader

//#version 330

attribute vec2 vVertex;

varying vec3 vPosition;

uniform mat4 ModelViewProjectionMatrix;

const float z = 0.0;

void main() {
  gl_Position = ModelViewProjectionMatrix * vec4(vVertex, z - 200.0, 1.0);
  vPosition = gl_Position.xyz;
}

