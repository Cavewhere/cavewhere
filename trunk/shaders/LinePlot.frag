#version 330

in float depth;

out vec4 fragColor;

void main(void)
{
    fragColor = vec4(depth - 0.2, depth - 0.2, 1.0, 1.0);
}
