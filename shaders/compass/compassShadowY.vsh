/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

/**
  This is the shader for rendering the compass rose's shadow

  This is a simple shader that simply maps the rectangle.

  This shader assumes that the qt_Vertex is equal to the texCoords
  */

attribute highp vec2 qt_Vertex;
uniform highp mat4 qt_ModelViewProjectionMatrix;
varying highp vec2 qt_TexCoord0;
varying vec2 v_blurTexCoords[14];

void main(void)
{
    qt_TexCoord0 = qt_Vertex.xy;
    gl_Position = qt_ModelViewProjectionMatrix * vec4(qt_Vertex, 0.0, 1.0);

    v_blurTexCoords[ 0] = qt_TexCoord0 + vec2(0.0, -0.028);
    v_blurTexCoords[ 1] = qt_TexCoord0 + vec2(0.0, -0.024);
    v_blurTexCoords[ 2] = qt_TexCoord0 + vec2(0.0, -0.020);
    v_blurTexCoords[ 3] = qt_TexCoord0 + vec2(0.0, -0.016);
    v_blurTexCoords[ 4] = qt_TexCoord0 + vec2(0.0, -0.012);
    v_blurTexCoords[ 5] = qt_TexCoord0 + vec2(0.0, -0.008);
    v_blurTexCoords[ 6] = qt_TexCoord0 + vec2(0.0, -0.004);
    v_blurTexCoords[ 7] = qt_TexCoord0 + vec2(0.0, 0.004);
    v_blurTexCoords[ 8] = qt_TexCoord0 + vec2(0.0, 0.008);
    v_blurTexCoords[ 9] = qt_TexCoord0 + vec2(0.0, 0.012);
    v_blurTexCoords[10] = qt_TexCoord0 + vec2(0.0, 0.016);
    v_blurTexCoords[11] = qt_TexCoord0 + vec2(0.0, 0.020);
    v_blurTexCoords[12] = qt_TexCoord0 + vec2(0.0, 0.024);
    v_blurTexCoords[13] = qt_TexCoord0 + vec2(0.0, 0.028);

}
