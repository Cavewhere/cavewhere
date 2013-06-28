attribute highp vec3 qt_Vertex;
attribute highp vec3 qt_Color;

varying vec3 color;

uniform highp mat4 qt_ModelViewProjectionMatrix;

void main(void)
{
    color = qt_Color;
    gl_Position = qt_ModelViewProjectionMatrix * vec4(qt_Vertex, 1.0);
}
