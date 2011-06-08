// simple vertex shader

#version 330

in vec2 vVertex;

out vec3 vPosition;
out vec3 vNormal;

uniform float vAngle; //For testing
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;

const float z = 0.0;

void main() {
  
  //vec4 globalPos = ModelMatrix * vec4(vVertex, 0.0, 1.0);

 // float x = vVertex.x;
 // float y = vVertex.y * sqrt(abs(vAngle) * cos(vAngle)); //vAngle * cos(cos(vAngle) * vVertex.y;

/*
  float z =  100.0 * cos( globalPos.x / 80.0 ); //2 / globalPos.x);
  z += 100.0 * cos (globalPos.y / 80.0 );
  z += -abs(globalPos.y / globalPos.x);
  z += -abs(globalPos.x / globalPos.y);
  z += -abs(globalPos.y / (2.0 * globalPos.x));
  z += pow(globalPos.x + globalPos.y, 2.0) / 500.0;
  z += pow(globalPos.x - globalPos.y, 2.0) / 500.0;
  //z += 10 * noise_new(globalPos.xy);
  z += sin(vAngle * 3.0) * abs(globalPos.x / 10.0); 
  z += cos(vAngle * 2.0) * abs(globalPos.y / 10.0); 
  z += cos(vAngle * 3.0) * abs(globalPos.y / 100.0); 
*/

  //vPosition = globalPos.xyz;

  //z += sin(vAngle * 1.0) * abs(globalPos.y / 100.0);  
// z += random(sin(vAngle));

/*
  float z = .0001 * (pow(globalPos.y,3) + 100 * pow(globalPos.y, 2));
  z += .0001 * (pow(globalPos.x,3) + 100 * pow(globalPos.x, 2));
  z = -abs(z);
*/
/*
  float z = -abs(.2 * pow(globalPos.y,3) + .003 * pow(globalPos.y,2)) / 100000 + -abs(globalPos.x * globalPos.x / 100) + 
sin(globalPos.y) + sin(globalPos.x) + 
  -abs(globalPos.y / globalPos.x + 
  globalPos.x / globalPos.y +
  2 * globalPos.x / globalPos.y); //cos(vAngle) * globalPos.x + sin(vAngle) * globalPos.y;
*/
  
  
  gl_Position = ModelViewProjectionMatrix * vec4(vVertex, z, 1.0);
  


  //gl_Position = ModelViewProjectionMatrix * vec4(vVertex, z, 1.0);
}

