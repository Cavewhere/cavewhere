// // simple fragment shader
//#version 440 core

// #ifdef GL_ES
// precision highp float;
// #endif

// //varying vec3 gTriangleDistance;
// //varying vec3 vPosition;
// varying vec2 vTexCoord;
// //varying float elevation;

// //out vec4 fragColor;

// //uniform float vAngle;
// //uniform vec4 colorBG;
// layout (binding = 0) uniform sampler2D Texture;

// //float amplify(float d, float scale, float offset) {
// //  d = scale * d + offset;
// //  d = clamp(d, 0.0, 1.0);
// // // d = 1.0 - exp2(-2.0 * d * d);
// //  return d;
// //}

// void main() {
// //  float triangleDistance = min(gTriangleDistance.x, min(gTriangleDistance.y, gTriangleDistance.z));

//   //vec4 sauce = vec4(vTexCoord, 0.0, 1.0); //(0.5, 0.5, 1.0, 1.0);
//   vec4 textureSample = texture2D(Texture, vTexCoord);

// //  float normElevation = (elevation - 100.0) / (300.0 - 100.0);
// //  vec3 elevationColor = normElevation * vec3(1.0, 0.0, 0.0) + vec3(0.5, 0.5, 0.5);


// //  float amplifyValue = amplify(triangleDistance, 85.0, 0.0);
//   gl_FragColor = textureSample;

// //  gl_FragColor = (1.0 - amplifyValue) * vec4(0.0, 0.2, .0, 1.0) + amplifyValue * textureSample;
// }

#version 440 core

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec3 vPosition;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D Texture;
layout(std140, binding = 2) uniform ScrapBlock {
    mat4 modelMatrix;
    vec2 gridAngles; // radians
    vec2 padding;
};


const float devicePixelRatio = 1.0f;
const float contourSpacing1 = 0.1f;
const float contourWidth1 = 1.0f;
const float contourSpacing2 = 1.0f;
const float contourWidth2 = 1.5f;
const vec3 contourColor = vec3(0.15, 0.15, 0.15);

vec3 rotatePosition(vec3 position, float azimuth, float pitch) {

    float sinAz = sin(azimuth);
    float cosAz = cos(azimuth);
    mat3 rotZ = mat3(
        cosAz, -sinAz, 0.0,
        sinAz,  cosAz, 0.0,
        0.0,    0.0,   1.0
    );

    float sinPitch = sin(pitch);
    float cosPitch = cos(pitch);
    mat3 rotX = mat3(
        1.0,    0.0,      0.0,
        0.0, cosPitch, -sinPitch,
        0.0, sinPitch,  cosPitch
    );

    return rotX * (rotZ * position);
}

float contour(vec3 position, float spacing, float widthPx) {

    float gsize = 1.0 / spacing;
    const float offset = 0.5;

    vec3 f  = abs(fract(position * gsize - offset) - offset);
    vec3 df = fwidth(position * gsize);

    float mi = max(0.0, widthPx - 1.0);
    float ma = max(1.0, widthPx); // these should be uniforms
    vec3 g = clamp((f - df * mi) / (df * (ma - mi)), max(0.0, 1.0 - widthPx), 1.0);
    // float c = g.x * g.y;
    float c = g.y;
    // float c = g.y * g.z;

    return c;
}

void main() {

    vec3 rotatedPosition = rotatePosition(vPosition, gridAngles.x, gridAngles.y);

    float c = contour(rotatedPosition, contourSpacing1, contourWidth1 * devicePixelRatio)
            * contour(rotatedPosition, contourSpacing2, contourWidth2 * devicePixelRatio);
    float lineMask = 1.0 - c;

    vec4 textureSample = texture(Texture, vTexCoord);

    fragColor = mix(textureSample, vec4(contourColor, textureSample.a), lineMask);
    // fragColor = vec4(vTexCoord, 0.0, 1.0);
}
