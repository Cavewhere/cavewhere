attribute vec3 vVertex;
attribute vec2 vScrapTexCoords;

varying vec3 vPosition;
varying vec2 vTexCoord;

uniform mat4 ModelViewProjectionMatrix;

void main() {
  gl_Position = ModelViewProjectionMatrix * vec4(vVertex, 1.0);
  vPosition = gl_Position.xyz;
  vTexCoord = vScrapTexCoords;
}
