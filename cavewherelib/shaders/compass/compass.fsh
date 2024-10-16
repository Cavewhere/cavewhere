/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifdef GL_ES
precision highp float;
#endif

varying vec3 color;

void main(void)
{
    gl_FragColor = vec4(color, 1.0);
}
