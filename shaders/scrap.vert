/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

attribute vec3 vertexPosition;
attribute vec2 scrapTexCoord;

varying vec2 vTexCoord;

uniform mat4 modelView;
uniform mat3 modelViewNormal;
uniform mat4 modelViewProjection;
uniform vec2 texCoordsScale;

void main() {
    gl_Position = modelViewProjection * vec4(vertexPosition, 1.0);
    vTexCoord = texCoordsScale * scrapTexCoord;
}
