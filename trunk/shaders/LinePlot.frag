#version 330

in float depth;

out vec4 fragColor;

void main(void)
{

    fragColor = vec4(1.0, depth, depth, 1.0);
}
