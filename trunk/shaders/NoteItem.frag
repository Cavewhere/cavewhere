//#version 330

//The texture that'll be sampled
uniform sampler2D Texture;

//The texture coordinates
//in vec2 TexCoord;
varying vec2 TexCoord;

//The color that's written out
//out vec4 color;

void main(void)
{

    //OGL 3.3
    //color = texture2D(Texture, TexCoord);

  //  gl_FragColor = texture2D(Texture, TexCoord);
    gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
