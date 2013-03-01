//#version 330

attribute vec2 vVertex;

uniform mat4 ModelViewProjectionMatrix;
uniform vec2 CropArea;

varying vec2 TexCoord;


void main(void)
{
    gl_Position = ModelViewProjectionMatrix * vec4(vVertex, -0.1, 1.0);
    TexCoord = vec2(vVertex.xy * CropArea);
}

