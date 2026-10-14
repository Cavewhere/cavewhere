/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#version 440 core

layout(location = 0) flat in uint vType; // cwRenderLinePlot segment type

layout(location = 0) out vec4 fragColor;

// Indexed by segment type; must match cwRenderLinePlot::segmentColor.
const vec4 kTypeColors[7] = vec4[7](
    vec4(1.0, 0.0, 0.0, 1.0),    // centerline: red
    vec4(1.0, 0.6, 0.6, 1.0),    // splay: pink
    vec4(0.85, 0.85, 0.85, 1.0), // ring: light gray
    vec4(0.2, 0.45, 1.0, 1.0),   // frame: blue
    vec4(0.1, 0.8, 0.2, 1.0),    // reading: green
    vec4(1.0, 0.85, 0.0, 1.0),   // section: yellow
    vec4(1.0, 0.0, 1.0, 1.0)     // overshoot: magenta
);

void main(void)
{
    fragColor = kTypeColors[vType];
}
