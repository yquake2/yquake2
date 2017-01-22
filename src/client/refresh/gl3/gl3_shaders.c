/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2016-2017 Daniel Gibson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * OpenGL3 refresher: Handling shaders
 *
 * =======================================================================
 */

#include "header/local.h"

// TODO: remove eprintf() usage
#define eprintf(...)  R_Printf(PRINT_ALL, __VA_ARGS__)


#if 0
static const char* vertexSrc = MULTILINE_STRING(#version 150\n
		in vec2 position;
		// I renamed color to inColor and Color to passColor for more clarity
		in vec3 inColor;
		// same for texcoord -> inTexCoord, Textcoord -> passTexCoord
		in vec2 inTexCoord;

		out vec3 passColor;
		out vec2 passTexCoord;

		void main() {
			passColor = inColor;
			passTexCoord = inTexCoord;
			gl_Position = vec4(position, 0.0, 1.0);
		}
);

static const char* fragmentSrc = MULTILINE_STRING(#version 150\n
		in vec3 passColor; // I renamed color to passColor (it's from the vertex shader above)
		in vec2 passTexCoord; // same for Texcoord -> passTexCoord
		out vec4 outColor;

		uniform sampler2D tex;

		void main()
		{
			outColor = texture(tex, passTexCoord) * vec4(passColor, 1.0);
			//outColor = texture(tex, passTexCoord);
			//outColor = vec4(passColor, 1.0);
		}
);
#endif // 0

static GLuint
CompileShader(GLenum shaderType, const char* shaderSrc)
{
	GLuint shader = glCreateShader(shaderType);
	glShaderSource(shader, 1, &shaderSrc, NULL);
	glCompileShader(shader);
	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if(status != GL_TRUE)
	{
		char buf[2048];
		char* bufPtr = buf;
		int bufLen = sizeof(buf);
		GLint infoLogLength;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);
		if(infoLogLength >= bufLen)
		{
			bufPtr = malloc(infoLogLength+1);
			bufLen = infoLogLength+1;
			if(bufPtr == NULL)
			{
				bufPtr = buf;
				bufLen = sizeof(buf);
				eprintf("WARN: In CompileShader(), malloc(%d) failed!\n", infoLogLength+1);
			}
		}

		glGetShaderInfoLog(shader, bufLen, NULL, bufPtr);

		const char* shaderTypeStr = "";
		switch(shaderType)
		{
			case GL_VERTEX_SHADER:   shaderTypeStr = "Vertex"; break;
			case GL_FRAGMENT_SHADER: shaderTypeStr = "Fragment"; break;
			case GL_GEOMETRY_SHADER: shaderTypeStr = "Geometry"; break;
			/* not supported in OpenGL3.2 and we're unlikely to need/use them anyway
			case GL_COMPUTE_SHADER:  shaderTypeStr = "Compute"; break;
			case GL_TESS_CONTROL_SHADER:    shaderTypeStr = "TessControl"; break;
			case GL_TESS_EVALUATION_SHADER: shaderTypeStr = "TessEvaluation"; break;
			*/
		}
		eprintf("ERROR: Compiling %s Shader failed: %s\n", shaderTypeStr, bufPtr);
		glDeleteShader(shader);

		if(bufPtr != buf)  free(bufPtr);

		return 0;
	}

	return shader;
}

static GLuint
CreateShaderProgram(int numShaders, const GLuint* shaders)
{
	int i=0;
	GLuint shaderProgram = glCreateProgram();
	if(shaderProgram == 0)
	{
		eprintf("ERROR: Couldn't create a new Shader Program!\n");
		return 0;
	}

	for(i=0; i<numShaders; ++i)
	{
		glAttachShader(shaderProgram, shaders[i]);
	}

	// the following line is not necessary/implicit (as there's only one output)
	// glBindFragDataLocation(shaderProgram, 0, "outColor"); XXX would this even be here?

	glLinkProgram(shaderProgram);

	GLint status;
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &status);
	if(status != GL_TRUE)
	{
		char buf[2048];
		char* bufPtr = buf;
		int bufLen = sizeof(buf);
		GLint infoLogLength;
		glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &infoLogLength);
		if(infoLogLength >= bufLen)
		{
			bufPtr = malloc(infoLogLength+1);
			bufLen = infoLogLength+1;
			if(bufPtr == NULL)
			{
				bufPtr = buf;
				bufLen = sizeof(buf);
				eprintf("WARN: In CreateShaderProgram(), malloc(%d) failed!\n", infoLogLength+1);
			}
		}

		glGetProgramInfoLog(shaderProgram, bufLen, NULL, bufPtr);

		eprintf("ERROR: Linking shader program failed: %s\n", bufPtr);

		glDeleteProgram(shaderProgram);

		if(bufPtr != buf)  free(bufPtr);

		return 0;
	}

	for(i=0; i<numShaders; ++i)
	{
		// after linking, they don't need to be attached anymore.
		// no idea  why they even are, if they don't have to..
		glDetachShader(shaderProgram, shaders[i]);
	}

	return shaderProgram;
}

#define MULTILINE_STRING(...) #__VA_ARGS__

static const char* vertexSrc2D = MULTILINE_STRING(#version 150\n
		in vec2 texCoord;
		in vec2 position;

		uniform mat4 trans;

		out vec2 passTexCoord;

		void main()
		{
			gl_Position = trans * vec4(position, 0.0, 1.0);
			passTexCoord = texCoord;
		}
);

static const char* fragmentSrc2D = MULTILINE_STRING(#version 150\n
		in vec2 passTexCoord;

		uniform sampler2D tex;

		out vec4 outColor;

		void main()
		{
			// TODO: gamma, intensity
			vec4 texel = texture(tex, passTexCoord);
			if(texel.a < 0.666)
				discard;
			outColor = texel;
		}
);

// 2D color only rendering, GL3_Draw_Fill(), GL3_Draw_FadeScreen()
static const char* vertexSrc2Dcolor = MULTILINE_STRING(#version 150\n
		in vec4 color;
		in vec2 position;

		uniform mat4 trans;

		out vec4 passColor;

		void main()
		{
			gl_Position = trans * vec4(position, 0.0, 1.0);
			passColor = color;
		}
);

static const char* fragmentSrc2Dcolor = MULTILINE_STRING(#version 150\n
		in vec4 passColor;

		uniform sampler2D tex;

		out vec4 outColor;

		void main()
		{
			// TODO: gamma, intensity? (not sure we need that here)
			outColor = passColor;
		}
);

#undef MULTILINE_STRING

static qboolean
initShader2D(gl3ShaderInfo_t* shaderInfo, const char* vertSrc, const char* fragSrc)
{
	GLuint shaders2D[2] = {0};
	GLint i = -1;
	GLuint prog = 0;

	if(shaderInfo->shaderProgram != 0)
	{
		R_Printf(PRINT_ALL, "WARNING: calling initShader2D for gl3ShaderInfo_t that already has a shaderProgram!\n");
		glDeleteProgram(shaderInfo->shaderProgram);
	}

	shaderInfo->attribColor = shaderInfo->attribPosition = shaderInfo->attribTexCoord = -1;
	shaderInfo->uniTransMatrix = -1;
	shaderInfo->shaderProgram = 0;

	shaders2D[0] = CompileShader(GL_VERTEX_SHADER, vertSrc);
	if(shaders2D[0] == 0)  return false;

	shaders2D[1] = CompileShader(GL_FRAGMENT_SHADER, fragSrc);
	if(shaders2D[1] == 0)
	{
		glDeleteShader(shaders2D[0]);
		return false;
	}

	prog = CreateShaderProgram(2, shaders2D);

	// I think the shaders aren't needed anymore once they're linked into the program
	glDeleteShader(shaders2D[0]);
	glDeleteShader(shaders2D[1]);

	if(prog == 0)
	{
		return false;
	}

	shaderInfo->shaderProgram = prog;
	glUseProgram(prog);

	i = glGetAttribLocation(prog, "position");
	if( i == -1)
	{
		R_Printf(PRINT_ALL, "WARNING: Couldn't get 'position' attribute in shader\n");
		return false;
	}
	shaderInfo->attribPosition = i;

	// the following line will set it to -1 for the textured case, that's ok.
	shaderInfo->attribColor = glGetAttribLocation(prog, "color");
	// the following line will set it to -1 for the color-only case, that's ok.
	shaderInfo->attribTexCoord = glGetAttribLocation(prog, "texCoord");

	i = glGetUniformLocation(prog, "trans");
	if( i == -1)
	{
		R_Printf(PRINT_ALL, "WARNING: Couldn't get 'trans' uniform in shader\n");
		return false;
	}
	shaderInfo->uniTransMatrix = i;

	return true;
}

qboolean GL3_InitShaders(void)
{
	if(!initShader2D(&gl3state.si2D, vertexSrc2D, fragmentSrc2D))
	{
		R_Printf(PRINT_ALL, "WARNING: Failed to create shader program for textured 2D rendering!\n");
		return false;
	}
	if(!initShader2D(&gl3state.si2Dcolor, vertexSrc2Dcolor, fragmentSrc2Dcolor))
	{
		R_Printf(PRINT_ALL, "WARNING: Failed to create shader program for color-only 2D rendering!\n");
		return false;
	}

	return true;
}

void GL3_ShutdownShaders(void)
{
	if(gl3state.si2D.shaderProgram != 0)
		glDeleteProgram(gl3state.si2D.shaderProgram);
	memset(&gl3state.si2D, 0, sizeof(gl3ShaderInfo_t));

	if(gl3state.si2Dcolor.shaderProgram != 0)
		glDeleteProgram(gl3state.si2Dcolor.shaderProgram);
	memset(&gl3state.si2Dcolor, 0, sizeof(gl3ShaderInfo_t));
}
