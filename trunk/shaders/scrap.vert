attribute vec3 vVertex;

varying vec3 vPosition;

uniform mat4 ModelViewProjectionMatrix;

void main() {
  gl_Position = ModelViewProjectionMatrix * vec4(vVertex, 1.0);
  vPosition = gl_Position.xyz;
}
