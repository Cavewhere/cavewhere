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
  d = 1.0 - exp2(-2.0 * d * d);
  return d;
}

void main() {

 // vec4 color = vec4(sin(vAngle), gTriangleDistance.x, cos(vAngle), 1.0);
  float triangleDistance = min(gTriangleDistance.x, min(gTriangleDistance.y, gTriangleDistance.z));

  vec4 sauce = vec4(0.0, 1.0, 0.0, 1.0); //(0.5, 0.5, 1.0, 1.0);
  //vec4 sauce = vec4(colorBG, 1.0);
  gl_FragColor = amplify(triangleDistance, 85.0, -0.1) * sauce;
//  float r = abs(cos(vAngle) * (gPosition.x + gPosition.y) / 500.0);
// r += abs(sin(vAngle) * (gPosition.x - gPosition.y) / 500.0);
//r += abs(sin(vAngle) / 2.0 * (gPosition.x + gPosition.y) / 500.0);
//r += abs(cos(vAngle) / 10.0 * (gPosition.x - gPosition.y) / 500.0);
//  float g = 0.2 + cos(vAngle);
//  float b = abs(0.2 + sin(vAngle));

//  vec4 color = vec4(r, g, b, 1.0); //sin(vAngle), cos(vAngle), 1.0); //vPosition.y / 500.0, vPosition.z / 500.0, 1.0);
//  //fragColor =  amplify(triangleDistance, 85.0, -0.1) * color;
//    gl_FragColor = amplify(triangleDistance, 85.0, -0.1) * color;
}
