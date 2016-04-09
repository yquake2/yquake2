#include "header/local.h"

static GLhandleARB program_handle;
static GLhandleARB vertex_shader;

void
R_InitPathtracing(void)
{
	program_handle = qglCreateProgramObjectARB();
}

void
R_ShutdownPathtracing(void)
{
	qglDeleteObjectARB(program_handle);
}

