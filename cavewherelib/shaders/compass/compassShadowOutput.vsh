/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

attribute highp vec2 qt_Vertex;
uniform highp mat4 qt_ModelViewProjectionMatrix;
varying highp vec2 qt_TexCoord0;

void main(void)
{
    gl_Position = qt_ModelViewProjectionMatrix * vec4(qt_Vertex, 0.0, 1.0);
    qt_TexCoord0 = qt_Vertex;
}
