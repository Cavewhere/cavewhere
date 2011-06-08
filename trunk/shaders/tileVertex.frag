// simple fragment shader
#version 330


in vec3 gTriangleDistance;
in vec3 gPosition;

uniform float vAngle;

float amplify(float d, float scale, float offset) {
  d = scale * d + offset;
  d = clamp(d, 0.0, 1.0);
  d = 1.0 - exp2(-2.0 * d * d);
  return d;
}

void main() {

 // vec4 color = vec4(sin(vAngle), gTriangleDistance.x, cos(vAngle), 1.0);
  float triangleDistance = min(gTriangleDistance.x, min(gTriangleDistance.y, gTriangleDistance.z));

  //gl_FragColor = amplify(triangleDistance, 85.0, -0.1) * color;
  float r = abs(cos(vAngle) * (gPosition.x + gPosition.y) / 500.0);
 r += abs(sin(vAngle) * (gPosition.x - gPosition.y) / 500.0);  
r += abs(sin(vAngle) / 2 * (gPosition.x + gPosition.y) / 500.0);
r += abs(cos(vAngle) / 10 * (gPosition.x - gPosition.y) / 500.0);
  float g = .2 + cos(vAngle);
  float b = abs(.2 + sin(vAngle));

  vec4 color = vec4(r, g, b, 1.0); //sin(vAngle), cos(vAngle), 1.0); //vPosition.y / 500.0, vPosition.z / 500.0, 1.0); 
  gl_FragColor =  amplify(triangleDistance, 85.0, -0.1) * color;
}
