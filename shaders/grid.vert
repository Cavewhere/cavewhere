/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// simple vertex shader

//#version 140

attribute vec3 vertexPosition;

varying vec4 vPosition;
varying vec4 projectedPosition;

uniform mat4 model;
uniform mat4 modelView;
uniform mat3 modelViewNormal;
uniform mat4 modelViewProjection;

const float z = 0.0;

void main() {
  vec4 position =  vec4(vertexPosition, 1.0);
  projectedPosition = modelViewProjection * position;
  vPosition.xyz = vec4(model * position).xyz; //vertexPosition;
  gl_Position = projectedPosition;
}
