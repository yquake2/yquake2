#version 450
#extension GL_ARB_separate_shader_objects : enable

out gl_PerVertex {
	vec4 gl_Position;
};

void main() 
{
	vec4 positions[3] = {
		vec4(-1.0f, -1.0f, 0.0f, 1.0f),
		vec4(3.0f, -1.0f, 0.0f, 1.0f),
		vec4(-1.0f, 3.0f, 0.0f, 1.0f)
	};

	gl_Position = positions[gl_VertexIndex % 3];
}
