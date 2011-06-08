// these lines enable the geometry shader support.
#version 330
#extension GL_EXT_geometry_shader4 : enable

layout(triangles) in;
layout(line_strip, max_vertices=4) out;

out vec3 gTriangleDistance;

void main( void ) {
	for( int i = 0 ; i < gl_VerticesIn ; i++ ) {
	  gl_Position = gl_PositionIn[i];
	  gTriangleDistance = vec3(0.0, 0.0, 0.0);
	  EmitVertex();
	}
	
	gl_Position = gl_PositionIn[0];
	gTriangleDistance = vec3(0.0, 0.0, 0.0);
	EmitVertex();

	EndPrimitive();
}
