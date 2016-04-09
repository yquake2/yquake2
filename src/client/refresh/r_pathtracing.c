#include "header/local.h"

cvar_t *gl_pt_enable;

GLhandleARB pt_program_handle;

static GLhandleARB vertex_shader;
static GLhandleARB fragment_shader;

const GLcharARB* vertex_shader_source =
	"#version 120\n"
	"void main() { gl_Position = ftransform(); gl_TexCoord[0] = gl_MultiTexCoord0; }\n"
	"\n";

const GLcharARB* fragment_shader_source =
	"#version 120\n"
	"uniform sampler2D tex0;\n"
	"void main() { gl_FragColor = texture2D(tex0, gl_TexCoord[0].st); }\n"
	"\n";
	
void
R_InitPathtracing(void)
{
	GLint status;
	
	gl_pt_enable = Cvar_Get( "gl_pt_enable", "0", CVAR_ARCHIVE);

	pt_program_handle = qglCreateProgramObjectARB();
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
	
	qglAttachObjectARB(pt_program_handle, vertex_shader);
	qglAttachObjectARB(pt_program_handle, fragment_shader);
	
	qglLinkProgramARB(pt_program_handle);
	qglGetObjectParameterivARB(pt_program_handle, GL_OBJECT_LINK_STATUS_ARB, &status);
	
	if (status != GL_TRUE)
	{
		VID_Printf(PRINT_ALL, "R_InitPathtracing: Program failed to link\n");
		return;
	}
	
	qglUseProgramObjectARB(pt_program_handle);
	qglUniform1iARB(qglGetUniformLocationARB(pt_program_handle, "tex0"), 0);
	qglUseProgramObjectARB(0);
}

void
R_ShutdownPathtracing(void)
{
	qglDeleteObjectARB(vertex_shader);
	qglDeleteObjectARB(fragment_shader);
	qglDeleteObjectARB(pt_program_handle);
}

