//#version 330

#ifdef GL_ES
precision highp float;
#endif

varying float depth;

//out vec4 fragColor;

void main(void)
{
  //  fragColor = vec4(depth - 0.2, depth - 0.2, 1.0, 1.0);

    gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
   // gl_FragColor = vec4(depth - 0.2, depth - 0.2, 1.0, 1.0);
}
