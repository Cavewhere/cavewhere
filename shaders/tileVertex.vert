/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// simple vertex shader

//#version 140

attribute vec2 vVertex;

varying vec4 vPosition;
varying vec4 projectedPosition;

uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;
//uniform mat4 ModelViewMatrix;

const float z = 0.0;

void main() {
  vec4 position =  vec4(vVertex, z - 90.0, 1.0);
  gl_Position = ModelViewProjectionMatrix * position;
  projectedPosition = gl_Position;
  vPosition.xyz = vec4(ModelMatrix * position).xyz;
}
