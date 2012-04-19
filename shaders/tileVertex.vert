// simple vertex shader

//#version 140

attribute vec2 vVertex;

varying vec3 vPosition;

uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;

const float z = 0.0;

void main() {
  vec4 position =  vec4(vVertex, z - 90.0, 1.0);
  gl_Position = ModelViewProjectionMatrix * position;
  vPosition = vec4(ModelMatrix * position).xyz;
}

