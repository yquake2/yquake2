#include "header/local.h"

cvar_t *gl_pt_enable;

GLhandleARB pt_program_handle;
GLhandleARB pt_node_texture = 0;
GLhandleARB pt_child_texture = 0;
GLint pt_frame_counter_loc = -1;

static GLhandleARB vertex_shader;
static GLhandleARB fragment_shader;

static unsigned long int texture_width, texture_height;

static const GLcharARB* vertex_shader_source =
	"#version 120\n"
	"void main() { gl_Position = ftransform(); gl_TexCoord[0] = gl_MultiTexCoord0; gl_TexCoord[1] = gl_Vertex; gl_TexCoord[2].xyz = vec3(0.0, 0.0, 0.0);  gl_TexCoord[3] = gl_MultiTexCoord2; }\n"
	"\n";

static const GLcharARB* fragment_shader_source =
	"#version 120\n"
	"\n"
	"#define EPS    (1./32.)\n"
	"#define MAXT   (2048.)\n"
	"\n"
	"uniform sampler2D tex0;\n"
	"uniform sampler2D planes;\n"
	"uniform sampler2D branches;\n"
	"\n"
	"uniform int frame = 0;\n"
	"\n"
	"float pi = acos(-1.);\n"
	"float aoradius = 150.;\n"
	"float seed = 0.;\n"
	"float rand() { return fract(sin(seed++)*43758.5453123); }\n"
	"\n"
	"vec3 rp = gl_TexCoord[1].xyz;\n"
	"vec3 dir = normalize(gl_TexCoord[2].xyz);\n"
	"vec3 normal = gl_TexCoord[3].xyz;\n"
	"vec4 out_pln;\n"
	"\n"
	"\n"
	"bool traceRayShadow(vec3 org, vec3 dir, float t0, float max_t)\n"
	"{\n"
	"   vec2  other_node=vec2(0);\n"
	"   float other_t1=max_t;\n"
	"\n"
	"   while(t0<max_t)\n"
	"   {\n"
	"      vec2  node=other_node;\n"
	"      float t1=other_t1;\n"
	"      \n"
	"      other_node=vec2(0);\n"
	"      other_t1=max_t;\n"
	"\n"
	"      do{\n"
	"         vec4 pln=texture2D(planes,node);\n"
	"         vec4 children=texture2D(branches,node);\n"
	"         \n"
	"         float t=dot(pln.xyz,dir);\n"
	"\n"
	"         children=(t>0.0) ? children.zwxy : children.xyzw;\n"
	"\n"
	"         t=(pln.w-dot(pln.xyz,org)) / t;\n"
	"\n"
	"         if(t>t0)\n"
	"         {\n"
	"            node=children.xy;\n"
	"            \n"
	"            if(t<t1) \n"
	"            {\n"
	"               other_t1=t1;\n"
	"               t1=t;\n"
	"               other_node=children.zw;\n"
	"            }\n"
	"         }\n"
	"         else\n"
	"         {\n"
	"            node=children.zw;\n"
	"         }\n"
	"         \n"
	"         if(node.y==1.0)\n"
	"         {\n"
	"             return false;\n"
	"         }\n"
	"         \n"
	"      } while(node!=vec2(0.0));\n"
	"      \n"
	"      t0=t1+EPS;\n"
	"   }\n"
	"\n"
	"   return true;\n"
	"}\n"
	"\n"
	"void main()\n"
	"{\n"
	"   seed = gl_FragCoord.x * 89.9 + gl_FragCoord.y * 197.3 + frame*0.02;\n"
	"\n"
	"   rp+=normal*EPS;\n"
	"   \n"
	"   out_pln.xyz=normal;\n"
	"   out_pln.w=dot(rp,out_pln.xyz);\n"
	"\n"
	"   vec4 spln=out_pln;\n"
	"\n"
	"   // permutate components\n"
	"   vec3 b=vec3(spln.z,spln.x,spln.y);\n"
	"   \n"
	"   // tangents\n"
	"   vec3 u=normalize(cross(spln.xyz,b)),\n"
	"          v=cross(spln.xyz,u);\n"
	"     \n"
	"   vec3 r=vec3(0.0);\n"
	"   const int n=4;\n"
	"  \n"
	"   for(int i=0;i<n;++i)\n"
	"   {\n"
	"         float r1=2.0*pi*rand();\n"
	"         float r2=rand();\n"
	"         float r2s=sqrt(r2);\n"
	"\n"
	"         vec3 rd=u*cos(r1)*r2s + v*sin(r1)*r2s + spln.xyz*sqrt(1.0-r2);\n"
	"            \n"
	"         if(traceRayShadow(rp,rd,EPS*16,aoradius))\n"
	"            r+=vec3(1.0/float(n));\n"
	"   }\n"
	"\n"
	"	gl_FragColor.rgb = r;\n"
	"	gl_FragColor.a = 1.0;\n"
	"	gl_FragColor *= texture2D(tex0, gl_TexCoord[0].st);\n"
	"}\n"
	"\n";

static void
FreeModelData(void)
{
	if (pt_node_texture)
	{
		glDeleteTextures(1, &pt_node_texture);
		pt_node_texture = 0;
	}

	if (pt_child_texture)
	{
		glDeleteTextures(1, &pt_child_texture);
		pt_child_texture = 0;
	}
}
	
void
R_PreparePathtracer(void)
{
	unsigned long int num_texels;
	float *tex_node_data;
	unsigned char *tex_child_data;
	unsigned int i, j;
	mnode_t* in;
	
	FreeModelData();
	
	texture_width = 1;
	texture_height = 1;

	if (r_worldmodel == NULL)
	{
		VID_Printf(PRINT_ALL, "R_PreparePathtracer: r_worldmodel is NULL!\n");
		return;
	}
	
	num_texels = r_worldmodel->numnodes;
	
	if (num_texels == 0)
	{
		VID_Printf(PRINT_ALL, "R_PreparePathtracer: num_texels is zero!\n");
		return;
	}

   while (texture_width * texture_height < num_texels)
   {
      texture_width <<= 1;
		
      if (texture_width * texture_height >= num_texels)
         break;
		
      texture_height <<= 1;
   }
	
	num_texels = texture_width * texture_height;
	
	tex_node_data = (float*)Z_Malloc(num_texels * 4 * sizeof(float));
	tex_child_data = (unsigned char*)Z_Malloc(num_texels * 4);
	
   for (i = 0; i < r_worldmodel->numnodes; ++i)
   {
      in = r_worldmodel->nodes + r_worldmodel->firstnode + i;

      tex_node_data[i * 4 + 0] = in->plane->normal[0];
      tex_node_data[i * 4 + 1] = in->plane->normal[1];
      tex_node_data[i * 4 + 2] = in->plane->normal[2];
      tex_node_data[i * 4 + 3] = in->plane->dist;

		for (j = 0; j < 2; ++j)
		{
			if (in->children[j]->contents == -1)
			{
				tex_child_data[i * 4 + 0 + j * 2] = ((in->children[j] - (r_worldmodel->nodes + r_worldmodel->firstnode)) % texture_width) * 256 / texture_width;
				tex_child_data[i * 4 + 1 + j * 2] = ((in->children[j] - (r_worldmodel->nodes + r_worldmodel->firstnode)) / texture_width) * 256 / texture_height;
			}
			else
			{
				tex_child_data[i * 4 + 0 + j * 2] = tex_child_data[i * 4 + 1 + j * 2] = in->children[j]->contents == CONTENTS_SOLID ? 255 : 0;
			}
		}
   }
	
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	
	glGenTextures(1, &pt_node_texture);
	glBindTexture(GL_TEXTURE_2D, pt_node_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, texture_width, texture_height, 0, GL_RGBA, GL_FLOAT, tex_node_data);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenTextures(1, &pt_child_texture);
	glBindTexture(GL_TEXTURE_2D, pt_child_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture_width, texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_child_data);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	Z_Free(tex_node_data);
	Z_Free(tex_child_data);
}
	
static void
PrintObjectInfoLog(GLhandleARB object)
{
	GLint info_log_length = 0;
	GLcharARB *info_log_buffer = NULL;

	qglGetObjectParameterivARB(object, GL_OBJECT_INFO_LOG_LENGTH_ARB, &info_log_length);
	
	if (info_log_length > 0)
	{
		info_log_buffer = (GLcharARB*)Z_Malloc(info_log_length * sizeof(GLcharARB));
		qglGetInfoLogARB(object, info_log_length, NULL, info_log_buffer);
		VID_Printf(PRINT_ALL, info_log_buffer);
		Z_Free(info_log_buffer);
	}
}

void
R_InitPathtracing(void)
{
	GLint status = 0;
	
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
		PrintObjectInfoLog(vertex_shader);
		return;
	}

	qglShaderSourceARB(fragment_shader, 1, &fragment_shader_source, NULL);
	qglCompileShaderARB(fragment_shader);
	qglGetObjectParameterivARB(fragment_shader, GL_OBJECT_COMPILE_STATUS_ARB, &status);
	
	if (status != GL_TRUE)
	{
		VID_Printf(PRINT_ALL, "R_InitPathtracing: Fragment shader failed to compile\n");
		PrintObjectInfoLog(fragment_shader);
		return;
	}
	
	qglAttachObjectARB(pt_program_handle, vertex_shader);
	qglAttachObjectARB(pt_program_handle, fragment_shader);
	
	qglLinkProgramARB(pt_program_handle);
	qglGetObjectParameterivARB(pt_program_handle, GL_OBJECT_LINK_STATUS_ARB, &status);
	
	if (status != GL_TRUE)
	{
		VID_Printf(PRINT_ALL, "R_InitPathtracing: Program failed to link\n");
		PrintObjectInfoLog(pt_program_handle);
		return;
	}
	
	qglUseProgramObjectARB(pt_program_handle);
	qglUniform1iARB(qglGetUniformLocationARB(pt_program_handle, "tex0"), 0);
	qglUniform1iARB(qglGetUniformLocationARB(pt_program_handle, "planes"), 2);
	qglUniform1iARB(qglGetUniformLocationARB(pt_program_handle, "branches"), 3);
	pt_frame_counter_loc = qglGetUniformLocationARB(pt_program_handle, "frame");
	qglUseProgramObjectARB(0);
}

void
R_ShutdownPathtracing(void)
{
	FreeModelData();
	qglDeleteObjectARB(vertex_shader);
	qglDeleteObjectARB(fragment_shader);
	qglDeleteObjectARB(pt_program_handle);
}

