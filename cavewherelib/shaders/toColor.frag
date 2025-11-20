#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 pixelColor;
};
layout(binding = 1) uniform sampler2D source;
void main() {
    vec4 p = texture(source, qt_TexCoord0);
    //Alpha needs to be pre-multiplyed as stated in the
    vec3 finalPixelColor = pixelColor.rgb * p.a;
    fragColor = vec4(finalPixelColor, p.a) * qt_Opacity;
}
