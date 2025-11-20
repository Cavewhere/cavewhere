#version 440

layout(location = 0) in vec2 position;

layout(location = 0) out vec2 v_uv;

void main()
{
    gl_Position = vec4(position, 0.0, 1.0);
    v_uv = position * 0.5 + 0.5; // Map from [-1,1] to [0,1]
}
