/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

/**
  This is the shader for rendering the compass rose's shadow

  This shader does gaussian interpolation on the compass rose render to
  a framebuffer.

  cwGLShader will automatically set the define for the orinetation of this shader.
 */

uniform sampler2D qt_Texture0;
varying highp vec2 qt_TexCoord0;

//vec4 horizontalBlur() {
//    vec4 resultingColor = vec4(0.0);

//    vec2 texturesCoords[3];
//    texturesCoords[0] = qt_TexCoord0 + vec2(5.0/512.0, 0.0);
//    texturesCoords[1] = qt_TexCoord0;
//    texturesCoords[2] = qt_TexCoord0 + vec2(5.0/512.0, 0.0);

//    resultingColor += texture2D(qt_Texture0, texturesCoords[0]) * 0.25;
//    resultingColor += texture2D(qt_Texture0, texturesCoords[1]) * 0.5;
//    resultingColor += texture2D(qt_Texture0, texturesCoords[2]) * 0.25;

//    return resultingColor;
//}

//vec4 verticalBlur() {
//    return vec4(0.0);
//}


//void main(void)
//{
//    gl_FragColor = horizontalBlur(); //+ vec4(1.0, 0.0, 0.0, 1.0);

////    vec4 color = texture2D(qt_Texture0, qt_TexCoord0);
////    gl_FragColor = texture2D(qt_Texture0, qt_TexCoord0);
////    vec2 v_blurTexCoords[14];
////    v_blurTexCoords[ 0] = qt_TexCoord0 + vec2(-0.028, 0.0);
////    v_blurTexCoords[ 1] = qt_TexCoord0 + vec2(-0.024, 0.0);
////    v_blurTexCoords[ 2] = qt_TexCoord0 + vec2(-0.020, 0.0);
////    v_blurTexCoords[ 3] = qt_TexCoord0 + vec2(-0.016, 0.0);
////    v_blurTexCoords[ 4] = qt_TexCoord0 + vec2(-0.012, 0.0);
////    v_blurTexCoords[ 5] = qt_TexCoord0 + vec2(-0.008, 0.0);
////    v_blurTexCoords[ 6] = qt_TexCoord0 + vec2(-0.004, 0.0);
////    v_blurTexCoords[ 7] = qt_TexCoord0 + vec2( 0.004, 0.0);
////    v_blurTexCoords[ 8] = qt_TexCoord0 + vec2( 0.008, 0.0);
////    v_blurTexCoords[ 9] = qt_TexCoord0 + vec2( 0.012, 0.0);
////    v_blurTexCoords[10] = qt_TexCoord0 + vec2( 0.016, 0.0);
////    v_blurTexCoords[11] = qt_TexCoord0 + vec2( 0.020, 0.0);
////    v_blurTexCoords[12] = qt_TexCoord0 + vec2( 0.024, 0.0);
////    v_blurTexCoords[13] = qt_TexCoord0 + vec2( 0.028, 0.0);

////    gl_FragColor = vec4(0.0);
////    gl_FragColor += texture2D(qt_Texture0, v_blurTexCoords[ 0])*0.0044299121055113265;
////    gl_FragColor += texture2D(qt_Texture0, v_blurTexCoords[ 1])*0.00895781211794;
////    gl_FragColor += texture2D(qt_Texture0, v_blurTexCoords[ 2])*0.0215963866053;
////    gl_FragColor += texture2D(qt_Texture0, v_blurTexCoords[ 3])*0.0443683338718;
////    gl_FragColor += texture2D(qt_Texture0, v_blurTexCoords[ 4])*0.0776744219933;
////    gl_FragColor += texture2D(qt_Texture0, v_blurTexCoords[ 5])*0.115876621105;
////    gl_FragColor += texture2D(qt_Texture0, v_blurTexCoords[ 6])*0.147308056121;
////    gl_FragColor += texture2D(qt_Texture0, qt_TexCoord0       )*0.159576912161;
////    gl_FragColor += texture2D(qt_Texture0, v_blurTexCoords[ 7])*0.147308056121;
////    gl_FragColor += texture2D(qt_Texture0, v_blurTexCoords[ 8])*0.115876621105;
////    gl_FragColor += texture2D(qt_Texture0, v_blurTexCoords[ 9])*0.0776744219933;
////    gl_FragColor += texture2D(qt_Texture0, v_blurTexCoords[10])*0.0443683338718;
////    gl_FragColor += texture2D(qt_Texture0, v_blurTexCoords[11])*0.0215963866053;
////    gl_FragColor += texture2D(qt_Texture0, v_blurTexCoords[12])*0.00895781211794;
////    gl_FragColor += texture2D(qt_Texture0, v_blurTexCoords[13])*0.0044299121055113265;

//}


const float sigma = 5.0;     // The sigma value for the gaussian function: higher value means more blur
                         // A good value for 9x9 is around 3 to 5
                         // A good value for 7x7 is around 2.5 to 4
                         // A good value for 5x5 is around 2 to 3.5
                         // ... play around with this based on what you need :)

const float blurSize = 0.008;  // This should usually be equal to
                         // 1.0f / texture_pixel_width for a horizontal blur, and
                         // 1.0f / texture_pixel_height for a vertical blur.

//uniform sampler2D qt_Texture0;  // Texture that will be blurred by this shader

const float pi = 3.14159265;

// The following are all mutually exclusive macros for various
// seperable blurs of varying kernel size
#if defined(VERTICAL_BLUR_9)
const float numBlurPixelsPerSide = 8.0;
const vec2  blurMultiplyVec      = vec2(0.0, 1.0);
#elif defined(HORIZONTAL_BLUR_9)
const float numBlurPixelsPerSide = 8.0;
const vec2  blurMultiplyVec      = vec2(1.0, 0.0);
#elif defined(VERTICAL_BLUR_7)
const float numBlurPixelsPerSide = 3.0;
const vec2  blurMultiplyVec      = vec2(0.0, 1.0);
#elif defined(HORIZONTAL_BLUR_7)
const float numBlurPixelsPerSide = 3.0;
const vec2  blurMultiplyVec      = vec2(1.0, 0.0);
#elif defined(VERTICAL_BLUR_5)
const float numBlurPixelsPerSide = 2.0;
const vec2  blurMultiplyVec      = vec2(0.0, 1.0);
#elif defined(HORIZONTAL_BLUR_5)
const float numBlurPixelsPerSide = 2.0;
const vec2  blurMultiplyVec      = vec2(1.0, 0.0);
#else
// This only exists to get this shader to compile when no macros are defined
const float numBlurPixelsPerSide = 0.0;
const vec2  blurMultiplyVec      = vec2(0.0, 0.0);
#endif

void main() {

  // Incremental Gaussian Coefficent Calculation (See GPU Gems 3 pp. 877 - 889)
  vec3 incrementalGaussian;
  incrementalGaussian.x = 1.0 / (sqrt(2.0 * pi) * sigma);
  incrementalGaussian.y = exp(-0.5 / (sigma * sigma));
  incrementalGaussian.z = incrementalGaussian.y * incrementalGaussian.y;

  vec4 avgValue = vec4(0.0, 0.0, 0.0, 0.0);
  float coefficientSum = 0.0;

  // Take the central sample first...
  avgValue += texture2D(qt_Texture0, qt_TexCoord0.xy) * incrementalGaussian.x;
  coefficientSum += incrementalGaussian.x;
  incrementalGaussian.xy *= incrementalGaussian.yz;

//  for(float xi = 0; xi < 9.0; xi++) {
//      for(float yi = 0; yi < 9.0; yi++) {
//          avgValue += texture2D(qt_Texture0, qt_TexCoord0.xy - xi * blurSize *
//                                blurMultiplyVec) * incrementalGaussian.x;
//          avgValue += texture2D(qt_Texture0, qt_TexCoord0.xy + yi * blurSize *
//                                blurMultiplyVec) * incrementalGaussian.x;
//          coefficientSum += 2.0 * incrementalGaussian.x;
//          incrementalGaussian.xy *= incrementalGaussian.yz;
//      }
//  }

  // Go through the remaining 8 vertical samples (4 on each side of the center)
  for (float i = 1.0; i <= numBlurPixelsPerSide; i++) {
    avgValue += texture2D(qt_Texture0, qt_TexCoord0.xy - i * blurSize *
                          blurMultiplyVec) * incrementalGaussian.x;
    avgValue += texture2D(qt_Texture0, qt_TexCoord0.xy + i * blurSize *
                          blurMultiplyVec) * incrementalGaussian.x;
    coefficientSum += 2.0 * incrementalGaussian.x;
    incrementalGaussian.xy *= incrementalGaussian.yz;
  }

  gl_FragColor = avgValue / coefficientSum;
}
