//#version 330

#ifdef GL_ES
precision highp float;
#endif

//The texture that'll be sampled
uniform sampler2D Texture;


//The texture coordinates
//in vec2 TexCoord;
varying vec2 TexCoord;

//The color that's written out
//out vec4 color;

void main(void)
{
    gl_FragColor = texture2D(Texture, TexCoord);
}
