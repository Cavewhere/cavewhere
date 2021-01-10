#version 150 core

uniform sampler2D diffuseTexture;

in vec3 position;
in vec2 texCoord;

out vec4 fragColor;

void main()
{
    float darken = gl_FrontFacing ? 1.0 : 0.75;
    fragColor = darken * texture( diffuseTexture, texCoord );
}
