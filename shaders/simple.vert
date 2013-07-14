/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

in vec3 Vertex;
uniform mat4 ModelViewProjectionMatrix;

void main(void)
{
    gl_Position = ModelViewProjectionMatrix * Vertex;
}
