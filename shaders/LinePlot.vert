/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//#version 330

attribute vec3 vVertex;
varying float depth;

uniform mat4 ModelViewProjectionMatrix;
uniform float MaxZValue;
uniform float MinZValue;

void main(void)
{
    depth = (vVertex.z - MinZValue) / (MaxZValue - MinZValue); //Normalize the z value
    gl_Position = ModelViewProjectionMatrix * vec4(vVertex, 1.0);
}
