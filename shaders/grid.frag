// simple fragment shader
//#version 140

#ifdef GL_ES
precision highp float;
#extension GL_OES_standard_derivatives : enable
#endif

//varying vec3 gTriangleDistance;
//varying vec3 gPosition;

varying vec4 vPosition;
varying vec4 projectedPosition;

//const float realSize = 10.0;
//const float gsize = 1.0 / realSize;
//const float gwidth = 1.0;


//out vec4 fragColor;

//uniform float vAngle;
//uniform vec4 colorBG;

//float amplify(float d, float scale, float offset) {
//  d = scale * d + offset;
//  d = clamp(d, 0.0, 1.0);
////  d = 1.0 - exp2(-2.0 * d * d);
//  return d;
//}

float contour(float spacing, float widthPx) {

    float gsize = 1.0 / spacing;
    const float offset = 0.5;

    vec3 f  = abs(fract (vPosition.xyz * gsize - offset) - offset);
    vec3 df = fwidth(vPosition.xyz * gsize);
//    vec3 g = smoothstep(-widthPx*df,widthPx*df , f);

    float mi=max(0.0,widthPx-1.0), ma=max(1.0,widthPx);//should be uniforms
    vec3 g=clamp((f-df*mi)/(df*(ma-mi)),max(0.0,1.0-widthPx),1.0);//max(0.0,1.0-gwidth) should also be sent as uniform
    float c = g.x * g.y;

    return c;
}

void main() {

 // vec4 color = vec4(sin(vAngle), gTriangleDistance.x, cos(vAngle), 1.0);
//  float triangleDistance = min(gTriangleDistance.x, min(gTriangleDistance.y, gTriangleDistance.z));

//  vec3 sauce = vec3(0.19608, 0.22745, 0.26275); //(0.5, 0.5, 1.0, 1.0);
//  gl_FragColor = vec4(amplify(triangleDistance, 85.0, -0.1) * sauce, 1.0);

//    if(mod(vPosition.x, 10.0) < 0.1 || mod(vPosition.y, 10.0) < 0.1) {
//        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
//    } else {
//        gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
//    }

//    vec3 f  = abs(fract (vPosition * gsize)-0.5);
//    vec3 df = fwidth(vPosition * gsize);
//    float mi=max(0.0,gwidth-1.0), ma=max(1.0,gwidth);//should be uniforms
//    vec3 g=clamp((f-df*mi)/(df*(ma-mi)),max(0.0,1.0-gwidth),1.0);//max(0.0,1.0-gwidth) should also be sent as uniform
//    float c = g.x;
//    gl_FragColor = vec4(c, c, c, 1.0);
//    gl_FragColor = gl_FragColor * gl_Color;

//    if(vPosition.x > 10.0) {
//        gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
//        return;
//    }

//    vec3 f  = fract (vPosition * .01);
//    vec3 df = fwidth(vPosition * .01);

//    vec3 g = smoothstep(df * 1.0, df * 2.0, f);

//    float c = g.x * g.y; // * g.z;

//    gl_FragColor = vec4(c, c, c, 1.0);


    //float depth = ((projectedPosition.z / projectedPosition.w) + 1.0) * 0.5 / 7.5;
    //const vec4 planeColor = vec4(0.5, 0.5, 0.5, 1.0);
    const vec4 planeColor = vec4(1.0, 1.0, 1.0, 0.0);
    float c = contour(100.0, 1.0) * contour(1000.0, 1.5);

    gl_FragColor = vec4(c, c, c, 1.0)* planeColor;
 //  gl_FragColor = vec4(depth, depth, depth, 1.0);

}
