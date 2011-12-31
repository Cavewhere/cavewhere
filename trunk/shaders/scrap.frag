// simple fragment shader
//#version 330


varying vec3 gTriangleDistance;
varying vec3 gPosition;
varying vec2 gTexCoord;

//out vec4 fragColor;

uniform float vAngle;
uniform vec4 colorBG;
uniform sampler2D Texture;

float amplify(float d, float scale, float offset) {
  d = scale * d + offset;
  d = clamp(d, 0.0, 1.0);
 // d = 1.0 - exp2(-2.0 * d * d);
  return d;
}

void main() {
  float triangleDistance = min(gTriangleDistance.x, min(gTriangleDistance.y, gTriangleDistance.z));

  vec4 sauce = vec4(gTexCoord, 0.0, 1.0); //(0.5, 0.5, 1.0, 1.0);
  vec4 textureSample = texture2D(Texture, gTexCoord);
  gl_FragColor = amplify(triangleDistance, 85.0, 0.0) * textureSample;
}
