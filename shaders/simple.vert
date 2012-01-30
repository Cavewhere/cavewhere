in vec3 Vertex;
uniform mat4 ModelViewProjectionMatrix;

void main(void)
{
    gl_Position = ModelViewProjectionMatrix * Vertex;
}
