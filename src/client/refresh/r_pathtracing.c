#include "header/local.h"

static GLhandleARB program_handle;
static GLhandleARB vertex_shader;
static GLhandleARB fragment_shader;

const GLcharARB* vertex_shader_source =
	"#version 120\n"
	"void main() { gl_Position = ftransform(); }\n"
	"\n";

const GLcharARB* fragment_shader_source =
	"#version 120\n"
	"void main() { gl_FragColor = vec4(1.0, 0.2, 1.0, 1.0); }\n"
	"\n";
	
void
R_InitPathtracing(void)
{
	GLint status;
	
	program_handle = qglCreateProgramObjectARB();
	vertex_shader = qglCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
	fragment_shader = qglCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
	
	qglShaderSourceARB(vertex_shader, 1, &vertex_shader_source, NULL);
	qglCompileShaderARB(vertex_shader);
	qglGetObjectParameterivARB(vertex_shader, GL_OBJECT_COMPILE_STATUS_ARB, &status);
	
	if (status != GL_TRUE)
	{
		VID_Printf(PRINT_ALL, "R_InitPathtracing: Vertex shader failed to compile\n");
		return;
	}

	qglShaderSourceARB(fragment_shader, 1, &fragment_shader_source, NULL);
	qglCompileShaderARB(fragment_shader);
	qglGetObjectParameterivARB(fragment_shader, GL_OBJECT_COMPILE_STATUS_ARB, &status);
	
	if (status != GL_TRUE)
	{
		VID_Printf(PRINT_ALL, "R_InitPathtracing: Fragment shader failed to compile\n");
		return;
	}
	
	qglAttachObjectARB(program_handle, vertex_shader);
	qglAttachObjectARB(program_handle, fragment_shader);
	
	qglLinkProgramARB(program_handle);
	qglGetObjectParameterivARB(program_handle, GL_OBJECT_LINK_STATUS_ARB, &status);
	
	if (status != GL_TRUE)
	{
		VID_Printf(PRINT_ALL, "R_InitPathtracing: Program failed to link\n");
		return;
	}
}

void
R_ShutdownPathtracing(void)
{
	qglDeleteObjectARB(program_handle);
	qglDeleteObjectARB(vertex_shader);
	qglDeleteObjectARB(fragment_shader);
}

