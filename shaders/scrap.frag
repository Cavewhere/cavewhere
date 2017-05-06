/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// simple fragment shader
//#version 330

#ifdef GL_ES
precision highp float;
#endif

varying vec2 vTexCoord;
uniform sampler2D scrapTexture;

void main() {
  vec4 textureSample = texture2D(scrapTexture, vTexCoord);
  gl_FragColor = textureSample;
}
