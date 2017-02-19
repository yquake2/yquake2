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


static GLuint
CompileShader(GLenum shaderType, const char* shaderSrc, const char* shaderSrc2)
{
	GLuint shader = glCreateShader(shaderType);

	const char* sources[2] = { shaderSrc, shaderSrc2 };
	int numSources = shaderSrc2 != NULL ? 2 : 1;

	glShaderSource(shader, numSources, sources, NULL);
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

	// make sure all shaders use the same attribute locations for common attributes
	// (so the same VAO can easily be used with different shaders)
	glBindAttribLocation(shaderProgram, GL3_ATTRIB_POSITION, "position");
	glBindAttribLocation(shaderProgram, GL3_ATTRIB_TEXCOORD, "texCoord");
	glBindAttribLocation(shaderProgram, GL3_ATTRIB_LMTEXCOORD, "lmTexCoord");

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
		in vec2 position; // GL3_ATTRIB_POSITION
		in vec2 texCoord; // GL3_ATTRIB_TEXCOORD

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
		uniform float gamma; // this is 1.0/vid_gamma
		uniform float intensity;

		out vec4 outColor;

		void main()
		{
			vec4 texel = texture(tex, passTexCoord);
			// the gl1 renderer used glAlphaFunc(GL_GREATER, 0.666);
			// and glEnable(GL_ALPHA_TEST); for 2D rendering
			// this should do the same
			if(texel.a <= 0.666)
				discard;

			// apply gamma correction and intensity
			texel.rgb *= intensity;
			outColor.rgb = pow(texel.rgb, vec3(gamma));
			outColor.a = texel.a; // I think alpha shouldn't be modified by gamma and intensity
		}
);

// 2D color only rendering, GL3_Draw_Fill(), GL3_Draw_FadeScreen()
static const char* vertexSrc2Dcolor = MULTILINE_STRING(#version 150\n
		in vec2 position; // GL3_ATTRIB_POSITION

		uniform mat4 trans;

		void main()
		{
			gl_Position = trans * vec4(position, 0.0, 1.0);
		}
);

static const char* fragmentSrc2Dcolor = MULTILINE_STRING(#version 150\n

		uniform float gamma; // this is 1.0/vid_gamma
		uniform float intensity;
		uniform vec4 color;

		out vec4 outColor;

		void main()
		{
			vec3 col = color.rgb * intensity;
			outColor.rgb = pow(col, vec3(gamma));
			outColor.a = color.a;
		}
);

static const char* vertexCommon3D = MULTILINE_STRING(#version 150\n

		in vec3 position;   // GL3_ATTRIB_POSITION
		in vec2 texCoord;   // GL3_ATTRIB_TEXCOORD
		in vec2 lmTexCoord; // GL3_ATTRIB_LMTEXCOORD

		out vec2 passTexCoord;

		// TODO: uniform blocks
);

static const char* fragmentCommon3D = MULTILINE_STRING(#version 150\n

		in vec2 passTexCoord;

		out vec4 outColor;

		// TODO: uniform blocks
);

static const char* vertexSrc3D = MULTILINE_STRING(

		uniform mat4 transProj;
		uniform mat4 transModelView;

		void main()
		{
			gl_Position = transProj * transModelView * vec4(position, 1.0);
			passTexCoord = texCoord;
		}
);

static const char* fragmentSrc3D = MULTILINE_STRING(

		uniform sampler2D tex;
		uniform float gamma; // this is 1.0/vid_gamma
		uniform float intensity;

		void main()
		{
			vec4 texel = texture(tex, passTexCoord);

			// TODO: something about GL_BLEND vs GL_ALPHATEST etc

			// apply gamma correction and intensity
			texel.rgb *= intensity;
			outColor.rgb = pow(texel.rgb, vec3(gamma));
			outColor.a = texel.a; // I think alpha shouldn't be modified by gamma and intensity
		}
);



#undef MULTILINE_STRING

/* TODO: UBOs
layout (std140) uniform common_data
{
	float gamma;
	float intensity;

	vec4 color; // really?

};

layout (std140) uniform for2D_data
{
	mat4 trans;
};

layout (std140) uniform for3D_data
{
	mat4 transProj;
	mat4 transModelView; // TODO: or maybe transViewProj; and transModel; ??
	float scroll; // for SURF_FLOWING
	float time; // or sth like this?
};
 */

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

	shaderInfo->uniColor = shaderInfo->uniProjMatrix = shaderInfo->uniModelViewMatrix = -1;
	shaderInfo->shaderProgram = 0;

	shaders2D[0] = CompileShader(GL_VERTEX_SHADER, vertSrc, NULL);
	if(shaders2D[0] == 0)  return false;

	shaders2D[1] = CompileShader(GL_FRAGMENT_SHADER, fragSrc, NULL);
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

	shaderInfo->uniColor = glGetUniformLocation(prog, "color");

	i = glGetUniformLocation(prog, "trans");
	if( i == -1)
	{
		R_Printf(PRINT_ALL, "WARNING: Couldn't get 'trans' uniform in shader\n");
		return false;
	}
	shaderInfo->uniProjMatrix = i;

	return true;
}

static qboolean
initShader3D(gl3ShaderInfo_t* shaderInfo, const char* vertSrc, const char* fragSrc)
{
	GLuint shaders3D[2] = {0};
	GLint i = -1;
	GLuint prog = 0;

	if(shaderInfo->shaderProgram != 0)
	{
		R_Printf(PRINT_ALL, "WARNING: calling initShader3D for gl3ShaderInfo_t that already has a shaderProgram!\n");
		glDeleteProgram(shaderInfo->shaderProgram);
	}

	shaderInfo->uniColor = shaderInfo->uniProjMatrix = shaderInfo->uniModelViewMatrix = -1;
	shaderInfo->shaderProgram = 0;

	shaders3D[0] = CompileShader(GL_VERTEX_SHADER, vertexCommon3D, vertSrc);
	if(shaders3D[0] == 0)  return false;

	shaders3D[1] = CompileShader(GL_FRAGMENT_SHADER, fragmentCommon3D, fragSrc);
	if(shaders3D[1] == 0)
	{
		glDeleteShader(shaders3D[0]);
		return false;
	}

	prog = CreateShaderProgram(2, shaders3D);

	// I think the shaders aren't needed anymore once they're linked into the program
	glDeleteShader(shaders3D[0]);
	glDeleteShader(shaders3D[1]);

	if(prog == 0)
	{
		return false;
	}

	shaderInfo->shaderProgram = prog;
	glUseProgram(prog);

	shaderInfo->uniColor = glGetUniformLocation(prog, "color");

	i = glGetUniformLocation(prog, "transProj");
	if( i == -1)
	{
		R_Printf(PRINT_ALL, "WARNING: Couldn't get 'trans' uniform in shader\n");
		return false;
	}
	shaderInfo->uniProjMatrix = i;
	i = glGetUniformLocation(prog, "transModelView");
	if( i == -1)
	{
		R_Printf(PRINT_ALL, "WARNING: Couldn't get 'trans' uniform in shader\n");
		return false;
	}
	shaderInfo->uniModelViewMatrix = i;

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
	if(!initShader3D(&gl3state.si3D, vertexSrc3D, fragmentSrc3D))
	{
		R_Printf(PRINT_ALL, "WARNING: Failed to create shader program for textured 3D rendering!\n");
		return false;
	}
	gl3state.currentShaderProgram = 0;


	GL3_SetGammaAndIntensity();

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

	if(gl3state.si3D.shaderProgram != 0)
		glDeleteProgram(gl3state.si3D.shaderProgram);
	memset(&gl3state.si3D, 0, sizeof(gl3ShaderInfo_t));
}

void GL3_SetGammaAndIntensity(void)
{
	float gamma = 1.0f/vid_gamma->value;
	float intens = intensity->value;
	int i=0;
	GLint progs[] = { gl3state.si2D.shaderProgram, gl3state.si2Dcolor.shaderProgram,
	                  gl3state.si3D.shaderProgram };

	for(i=0; i<sizeof(progs)/sizeof(progs[0]); ++i)
	{
		glUseProgram(progs[i]);
		GLint uni = glGetUniformLocation(progs[i], "gamma");
		if(uni != -1)
		{
			glUniform1f(uni, gamma);
		}

		uni = glGetUniformLocation(progs[i], "intensity");
		if(uni != -1)
		{
			glUniform1f(uni, intens);
		}
	}
}
