/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// simple fragment shader
//#version 140

#ifdef GL_ES
precision highp float;
#extension GL_OES_standard_derivatives : enable
#endif

varying vec4 vPosition;
varying vec4 projectedPosition;

float contour(float spacing, float widthPx) {

    float gsize = 1.0 / spacing;
    const float offset = 0.5;

    vec3 f  = abs(fract (vPosition.xyz * gsize - offset) - offset);
    vec3 df = fwidth(vPosition.xyz * gsize);

    float mi=max(0.0,widthPx-1.0);
    float ma=max(1.0,widthPx);//should be uniforms
    vec3 g=clamp((f-df*mi)/(df*(ma-mi)),max(0.0,1.0-widthPx),1.0);//max(0.0,1.0-gwidth) should also be sent as uniform
    float c = g.x * g.y;

    return c;
}

void main() {

    float c = contour(100.0, 1.25) * contour(1000.0, 1.5);
    c = 1.0 - min(1.0, c);
    gl_FragColor = vec4(c, c, c, c) * vec4(0.0, 0.0, 0.0, 1.0);
    gl_FragDepth = gl_FragColor.a >= 0.001 ? gl_FragCoord.z : 1.0;
}
