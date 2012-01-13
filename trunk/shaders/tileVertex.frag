// simple fragment shader
//#version 330


varying vec3 gTriangleDistance;
varying vec3 gPosition;

//out vec4 fragColor;

uniform float vAngle;
uniform vec4 colorBG;

float amplify(float d, float scale, float offset) {
  d = scale * d + offset;
  d = clamp(d, 0.0, 1.0);
//  d = 1.0 - exp2(-2.0 * d * d);
  return d;
}

void main() {

 // vec4 color = vec4(sin(vAngle), gTriangleDistance.x, cos(vAngle), 1.0);
  float triangleDistance = min(gTriangleDistance.x, min(gTriangleDistance.y, gTriangleDistance.z));

  vec3 sauce = vec3(0.19608, 0.22745, 0.26275); //(0.5, 0.5, 1.0, 1.0);
  gl_FragColor = vec4(amplify(triangleDistance, 85.0, -0.1) * sauce, 1.0);
}
