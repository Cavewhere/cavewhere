/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// simple fragment shader

#version 440 core

layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec4 projectedPosition;

layout(location = 0) out vec4 fragColor;

// layout(std140, binding = 1) uniform FragmentUniforms {
//     float devicePixelRatio;
//     float contourSpacing1;
//     float contourWidth1;
//     float contourSpacing2;
//     float contourWidth2;
// };


const float devicePixelRatio = 1.0f;
const float contourSpacing1 = 100.0f;
const float contourWidth1 = 1.0f;
const float contourSpacing2 = 1000.0f;
const float contourWidth2 = 1.5f;


float contour(float spacing, float widthPx) {

    float gsize = 1.0 / spacing;
    const float offset = 0.5;

    vec3 f  = abs(fract(vPosition.xyz * gsize - offset) - offset);
    vec3 df = fwidth(vPosition.xyz * gsize);

    float mi = max(0.0, widthPx - 1.0);
    float ma = max(1.0, widthPx); // these should be uniforms
    vec3 g = clamp((f - df * mi) / (df * (ma - mi)), max(0.0, 1.0 - widthPx), 1.0);
    float c = g.x * g.y;
    // float c = g.x * g.y;

    return c;
}

void main() {

    float c = contour(contourSpacing1, contourWidth1 * devicePixelRatio)
            * contour(contourSpacing2, contourWidth2 * devicePixelRatio);
    float ca = 1.0 - min(1.0, c);
    fragColor = vec4(c, c, c, ca) * vec4(0.0, 0.0, 0.0, 1.0);
    // fragColor = vec4(1.0, 0.0, 0.0, 1.0);

    // if(vPosition.z >= 100.0) {
    //     fragColor = vec4(0.0, 1.0, 0.0, 1.0);
    // } else {

    //     // fragColor = vec4(mod(vPosition.xyz, 100.0), 1.0);
    //     fragColor =  vec4(1.0, 0.0, 0.0, 1.0);
    // }
}

