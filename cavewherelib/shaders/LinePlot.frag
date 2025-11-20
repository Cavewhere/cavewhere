/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440 core

// layout(location = 0) in float depth; // Replaces varying keyword

layout(location = 0) out vec4 fragColor;

void main(void)
{
    // Assign the fragment color based on depth value
    // fragColor = vec4(depth - 0.2, depth - 0.2, 1.0, 1.0);

    fragColor = vec4(1.0, 0.0, 0.0, 1.0); // Fixed color (red)
    // Uncomment the next line if depth-based coloring is needed:
    // fragColor = vec4(depth - 0.2, depth - 0.2, 1.0, 1.0);
}
