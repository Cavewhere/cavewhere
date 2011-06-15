#version 330

in vec3 vVertex;
out float depth;

uniform mat4 ModelViewProjectionMatrix;
uniform float MaxZValue;
uniform float MinZValue;

void main(void)
{
    depth = (vVertex.z - MinZValue) / (MaxZValue - MinZValue); //Normalize the z value
    gl_Position = ModelViewProjectionMatrix * vec4(vVertex, 1.0);
}
