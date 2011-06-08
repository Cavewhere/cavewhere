// simple geometry shader

// these lines enable the geometry shader support.
#version 330
#extension GL_EXT_geometry_shader4 : enable

layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

in vec3 vPosition[3];

out vec3 gTriangleDistance;
out vec3 gPosition;

//in vec3 vPosition;

void main( void )
{
	//for( int i = 0 ; i < gl_VerticesIn ; i++ )
	//{
	gl_Position = gl_PositionIn[0];
	gTriangleDistance = vec3(1.0, 0.0, 0.0);
	gPosition = vPosition[0];
	EmitVertex();

	gl_Position = gl_PositionIn[1];
	gTriangleDistance = vec3(0.0, 1.0, 0.0);
	gPosition = vPosition[1];
	EmitVertex();

	gl_Position = gl_PositionIn[2];
	gTriangleDistance = vec3(0.0, 0.0, 1.0);
	gPosition = vPosition[2];
	EmitVertex();
	
	
	//}
	//gl_Position = gl_PositionIn[0];
	//EmitVertex();

	EndPrimitive();
}
