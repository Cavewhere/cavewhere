/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//#version 330

//attribute vec3 vVertex;

attribute vec3 vertexPosition;
//attribute vec3 vertexPosition;
//varying float depth;

//uniform mat4 ModelViewProjectionMatrix;
//uniform float MaxZValue;
//uniform float MinZValue;

uniform mat4 modelView;
uniform mat3 modelViewNormal;
uniform mat4 modelViewProjection;

void main(void)
{
//    depth = (vVertex.z - MinZValue) / (MaxZValue - MinZValue); //Normalize the z value
    gl_Position = modelViewProjection * vec4(vertexPosition, 1.0);
//        gl_Position = vec4(vertexPosition, 1.0);
//    gl_Position.z -= 1e-4;
}
