#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;     // Offset 0, size 64 bytes
    float qt_Opacity;   // Offset 64, size 4 bytes
    vec3 padding;       // Offsets 68, 72, 76
    vec4 overlayColor;  // Offset 80, size 16 bytes
};

layout(binding = 1) uniform sampler2D source;

void main() {
    vec4 src = texture(source, qt_TexCoord0);
    vec4 col = vec4(overlayColor.rgb * src.a, src.a);
    fragColor = col * qt_Opacity;
}
