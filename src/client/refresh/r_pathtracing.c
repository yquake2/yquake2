#include "header/local.h"

#define PT_MAX_TRI_NODES		16384
#define PT_MAX_TRIANGLES		16384
#define PT_MAX_VERTICES			16384
#define PT_MAX_NODE_CHILDREN	4
#define PT_MAX_NODE_DEPTH		4

cvar_t *gl_pt_enable;

GLhandleARB pt_program_handle;
GLhandleARB pt_node_texture = 0;
GLhandleARB pt_child_texture = 0;

GLuint pt_node0_buffer = 0;
GLuint pt_node0_texture = 0;
GLuint pt_node1_buffer = 0;
GLuint pt_node1_texture = 0;
GLuint pt_triangle_buffer = 0;
GLuint pt_triangle_texture = 0;
GLuint pt_vertex_buffer = 0;
GLuint pt_vertex_texture = 0;

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
	"#define EPS	 (1./32.)\n"
	"#define MAXT	(2048.)\n"
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
	"	vec2  other_node=vec2(0);\n"
	"	float other_t1=max_t;\n"
	"\n"
	"	while(t0<max_t)\n"
	"	{\n"
	"		vec2  node=other_node;\n"
	"		float t1=other_t1;\n"
	"		\n"
	"		other_node=vec2(0);\n"
	"		other_t1=max_t;\n"
	"\n"
	"		do{\n"
	"			vec4 pln=texture2D(planes,node);\n"
	"			vec4 children=texture2D(branches,node);\n"
	"			\n"
	"			float t=dot(pln.xyz,dir);\n"
	"\n"
	"			children=(t>0.0) ? children.zwxy : children.xyzw;\n"
	"\n"
	"			t=(pln.w-dot(pln.xyz,org)) / t;\n"
	"\n"
	"			if(t>t0)\n"
	"			{\n"
	"				node=children.xy;\n"
	"				\n"
	"				if(t<t1) \n"
	"				{\n"
	"					other_t1=t1;\n"
	"					t1=t;\n"
	"					other_node=children.zw;\n"
	"				}\n"
	"			}\n"
	"			else\n"
	"			{\n"
	"				node=children.zw;\n"
	"			}\n"
	"			\n"
	"			if(node.y==1.0)\n"
	"			{\n"
	"				 return false;\n"
	"			}\n"
	"			\n"
	"		} while(node!=vec2(0.0));\n"
	"		\n"
	"		t0=t1+EPS;\n"
	"	}\n"
	"\n"
	"	return true;\n"
	"}\n"
	"\n"
	"void main()\n"
	"{\n"
	"	seed = gl_FragCoord.x * 89.9 + gl_FragCoord.y * 197.3 + frame*0.02;\n"
	"\n"
	"	rp+=normal*EPS;\n"
	"	\n"
	"	out_pln.xyz=normal;\n"
	"	out_pln.w=dot(rp,out_pln.xyz);\n"
	"\n"
	"	vec4 spln=out_pln;\n"
	"\n"
	"	// permutate components\n"
	"	vec3 b=vec3(spln.z,spln.x,spln.y);\n"
	"	\n"
	"	// tangents\n"
	"	vec3 u=normalize(cross(spln.xyz,b)),\n"
	"			 v=cross(spln.xyz,u);\n"
	"	  \n"
	"	vec3 r=vec3(0.0);\n"
	"	const int n=4;\n"
	"  \n"
	"	for(int i=0;i<n;++i)\n"
	"	{\n"
	"			float r1=2.0*pi*rand();\n"
	"			float r2=rand();\n"
	"			float r2s=sqrt(r2);\n"
	"\n"
	"			vec3 rd=u*cos(r1)*r2s + v*sin(r1)*r2s + spln.xyz*sqrt(1.0-r2);\n"
	"				\n"
	"			if(traceRayShadow(rp,rd,EPS*16,aoradius))\n"
	"				r+=vec3(1.0/float(n));\n"
	"	}\n"
	"\n"
	"	gl_FragColor.rgb = r;\n"
	"	gl_FragColor.a = 1.0;\n"
	"	gl_FragColor *= texture2D(tex0, gl_TexCoord[0].st);\n"
	"}\n"
	"\n";

	
typedef float vec4_t[4];
static vec4_t pt_lerped[MAX_VERTS];


typedef struct trinode_s
{
	qboolean leaf;
	short triangle_index;
	float aabb_min[3], aabb_max[3];
	float surface_area;
	struct trinode_s* children[PT_MAX_NODE_CHILDREN];
	int num_children;
	unsigned long int morton_code;
} trinode_t;

static trinode_t pt_trinodes[PT_MAX_TRI_NODES];
static trinode_t *pt_trinodes_ordered[PT_MAX_TRI_NODES];
static short pt_num_nodes = 0;
static short pt_num_triangles = 0;

static int pt_triangle_data[PT_MAX_TRIANGLES * 2];
static int pt_node0_data[PT_MAX_TRI_NODES * 4];
static int pt_node1_data[PT_MAX_TRI_NODES * 4];

static void
TriNodeClear(trinode_t *n)
{
	int i;
	n->leaf = true;
	n->triangle_index = 0;
	for (i = 0; i < 3; ++i)
	{
		n->aabb_min[i] = 1e9f;
		n->aabb_max[i] = -1e9f;
	}
	n->surface_area = 0;
	n->num_children = 0;
	n->morton_code = 0;
}

static trinode_t*
AllocateNode()
{
	trinode_t *n = pt_trinodes + pt_num_nodes;
	pt_trinodes_ordered[pt_num_nodes] = n;
	TriNodeClear(n);
	++pt_num_nodes;
	return n;
}

static float
TriNodeCalculateSurfaceArea(const trinode_t *n)
{
	int i;
	float aabb_size[3];
	for (i = 0; i < 3; ++i)
		aabb_size[i] = n->aabb_max[i] - n->aabb_min[i];
	return (aabb_size[0] * aabb_size[1] + aabb_size[1] * aabb_size[2] + aabb_size[2] * aabb_size[0]) * 2;
}

static unsigned long int
TriNodeCalculateMortonCode(const trinode_t *n)
{
	int i;
	
	unsigned long int box_center[3] = { (unsigned long int)((n->aabb_min[0] + n->aabb_max[0]) / 2.0),
													(unsigned long int)((n->aabb_min[1] + n->aabb_max[1]) / 2.0),
													(unsigned long int)((n->aabb_min[2] + n->aabb_max[2]) / 2.0) };

	unsigned long int bits = 0;

	/* Interleave the bits of each component of the center point. */
	for (i = 0; i < 32; ++i)
		bits |= (box_center[i % 3] & (1 << (i / 3))) << (i - i / 3);

	return bits;
}

static int
TriNodeSurfaceAreaComparator(void const *a, void const *b)
{
	trinode_t *n0 = *(trinode_t**)a;
	trinode_t *n1 = *(trinode_t**)b;
	return n0->surface_area > n1->surface_area;
}

static int
TriNodeMortonCodeComparator(void const *a, void const *b)
{
	trinode_t *n0 = *(trinode_t**)a;
	trinode_t *n1 = *(trinode_t**)b;
	return n0->morton_code < n1->morton_code;
}

static int
FloatBitsToInt(float x)
{
	return *(int*)&x;
}

static int
TriNodeWriteData(const trinode_t *n, int *node0_data, int *node1_data, int index)
{
	int i, c, prev_child_index;
	int child_index, m;
	int *n0 = node0_data + index * 4;
	int *n1 = node1_data + index * 4;
	int *pn0;
	float aabb_size[3];

	for (i = 0; i < 3; ++i)
		aabb_size[i] = n->aabb_max[i] - n->aabb_min[i];

	for (i = 0; i < 3; ++i)
	{
		n0[i] = FloatBitsToInt(aabb_size[i] / 2.0f);
		n1[i] = FloatBitsToInt((n->aabb_min[i] + n->aabb_max[i]) / 2.0f);
	}

	n0[3] = 0;

	if (n->leaf)
	{
		n1[3] = n->triangle_index;
		return 1;
	}

	n1[3] = -1;

	c = 1;
	prev_child_index = -1;
	
	for (i = 0; i < n->num_children; ++i)
	{
		child_index = index + c;
		m = TriNodeWriteData(n->children[i], node0_data, node1_data, child_index);

		if (m > 0)
		{
			if (prev_child_index != -1)
			{
				pn0 = node0_data + prev_child_index * 4;
				pn0[3] = child_index;
			}

			prev_child_index = child_index;
			c += m;
		}
	}

	pn0 = node0_data + prev_child_index * 4;
	pn0[3] = index + c;

	return c;
}

static int
WriteTriNodes(int *node0_data, int *node1_data, const trinode_t *nodes, int num_nodes)
{
	int i, m;
	int node_count = 0, prev_node_count = -1;
	int *pn0;
	
	for (i = 0; i < num_nodes; ++i)
	{
		m = TriNodeWriteData(nodes + i, node0_data, node1_data, node_count);
		if (m > 0)
		{
			if (node_count > 0)
			{
				pn0 = node0_data + prev_node_count * 4;
				pn0[3] = node_count;
			}
			prev_node_count = node_count;
			node_count += m;
		}
	}

	return node_count;
}

static void
DeleteBuffer(GLuint *h)
{
	if (*h)
	{
		if (qglDeleteBuffersARB)
			qglDeleteBuffersARB(1, h);
		else if(qglDeleteBuffers)
			qglDeleteBuffers(1, h);
		*h = 0;
	}
}

static void
DeleteTexture(GLuint *h)
{
	if (*h)
	{
		glDeleteTextures(1, h);
		*h = 0;
	}
}

static void
FreeModelData(void)
{
	DeleteTexture(&pt_node_texture);
	DeleteTexture(&pt_child_texture);

	DeleteBuffer(&pt_node0_buffer);
	DeleteTexture(&pt_node0_texture);
	DeleteBuffer(&pt_node1_buffer);
	DeleteTexture(&pt_node1_texture);
	DeleteBuffer(&pt_triangle_buffer);
	DeleteTexture(&pt_triangle_texture);
	DeleteBuffer(&pt_vertex_buffer);
	DeleteTexture(&pt_vertex_texture);
}

/* Applies a translation vector to the given 4x4 matrix in-place. */
static void
MatrixTranslate(float m[16], float x, float y, float z)
{
	m[12] += x;
	m[13] += y;
	m[14] += z;
}

/* Sets the given 4x4 matrix to the identity matrix. */
static void
MatrixIdentity(float m[16])
{
	int i;
	for (i = 0; i < 16; ++i)
		m[i] = (i % 5) == 0 ? 1 : 0;
}

/* Performs an algebraic 4x4 matrix multiplication by concatenating m1 to m0 and writing the result into mr. */
static void
MatrixApply(float mr[16], float m0[16], float m1[16])
{
	int i, j, k;
	for (i = 0; i < 4; ++i)
		for (j = 0; j < 4; ++j)
		{
			mr[i + j * 4] = 0;
			for (k = 0; k < 4; ++k)
			{
				mr[i + j * 4] += m0[i + k * 4] * m1[k + j * 4];
			}
		}
}

/* Applies a counter-clockwise rotation about the given axis to the given 4x4 matrix in-place. */
static void
MatrixRotateAxis(float m[16], int axis, float angle)
{
	float ca = cos(angle);
	float sa = sin(angle);
	float mt0[16];
	float mt1[16];

	memcpy(mt0, m, sizeof(mt0));
	
	MatrixIdentity(mt1);

	switch(axis)
	{
		case 0:
			/* X Axis */
			mt1[ 5] = ca;
			mt1[ 6] = sa;
			mt1[ 9] = -sa;
			mt1[10] = ca;
			break;

		case 1:
			/* Y Axis */
			mt1[ 0] = ca;
			mt1[ 2] = -sa;
			mt1[ 8] = sa;
			mt1[10] = ca;
			break;

		case 2:
			/* Z Axis */
			mt1[ 0] = ca;
			mt1[ 1] = sa;
			mt1[ 4] = -sa;
			mt1[ 5] = ca;
			break;
			
		default:
			return;
	}
	
	MatrixApply(m, mt0, mt1);
}

/* Constructs an interpolated mesh in worldspace to match the one which is drawn by R_DrawAliasModel. This function
	is mostly based on R_DrawAliasModel except that it does no drawing. */
static void
AddAliasModel(entity_t *entity, model_t *model)
{
	dmdl_t *alias_header;
	daliasframe_t *frame, *oldframe;
	dtrivertx_t *v, *ov;
	float frontlerp;
	vec3_t move, delta, vectors[3];
	vec3_t frontv, backv;
	int i, j, k;
	float *lerp;
	float transformation_matrix[16];
	vec4_t lerped_vertex;
	dtriangle_t *triangles, *tri;
	trinode_t *node;
		
	/* Construct a transformation matrix to match the one used for drawing entities. This is based on the GL matrix transformation
		code in R_DrawAliasModel and R_RotateForEntity. */
	
	entity->angles[PITCH] = -entity->angles[PITCH];
	MatrixIdentity(transformation_matrix);	
	MatrixTranslate(transformation_matrix, entity->origin[0], entity->origin[1], entity->origin[2]);
	MatrixRotateAxis(transformation_matrix, 2, entity->angles[1] * M_PI / 180.0);
	MatrixRotateAxis(transformation_matrix, 1, -entity->angles[0] * M_PI / 180.0);
	MatrixRotateAxis(transformation_matrix, 0, -entity->angles[2] * M_PI / 180.0);
	entity->angles[PITCH] = -entity->angles[PITCH];
	
	/* Get the alias model header. */
	
	alias_header = (dmdl_t *)model->extradata;
	
	/* Check the frame numbers in the same way that R_DrawAliasModel does. */
	
	if ((entity->frame >= alias_header->num_frames) ||
		(entity->frame < 0))
	{
		VID_Printf(PRINT_DEVELOPER, "Pathtracer: AddAliasModel %s: no such frame %d\n",
				model->name, entity->frame);
		entity->frame = 0;
		entity->oldframe = 0;
	}

	if ((entity->oldframe >= alias_header->num_frames) ||
		(entity->oldframe < 0))
	{
		VID_Printf(PRINT_DEVELOPER, "Pathtracer: AddAliasModel %s: no such oldframe %d\n",
				model->name, entity->oldframe);
		entity->frame = 0;
		entity->oldframe = 0;
	}

	/* Disable interpolation if the gl_lerpmodels Cvar is zero. */
	
	if (!gl_lerpmodels->value)
	{
		entity->backlerp = 0;
	}
	
	/* Get the frame headers and vertices for each frame to be blended together. */
	
	frame = (daliasframe_t *)((byte *)alias_header + alias_header->ofs_frames
							  + entity->frame * alias_header->framesize);
	v = frame->verts;

	oldframe = (daliasframe_t *)((byte *)alias_header + alias_header->ofs_frames
				+ entity->oldframe * alias_header->framesize);
	ov = oldframe->verts;

	/* Calculate the blending weight of the second frame. */
	
	frontlerp = 1.0 - entity->backlerp;

	/* move should be the delta back to the previous frame * backlerp */
	VectorSubtract(entity->oldorigin, entity->origin, delta);
	AngleVectors(entity->angles, vectors[0], vectors[1], vectors[2]);

	move[0] = DotProduct(delta, vectors[0]); /* forward */
	move[1] = -DotProduct(delta, vectors[1]); /* left */
	move[2] = DotProduct(delta, vectors[2]); /* up */

	VectorAdd(move, oldframe->translate, move);

	/* Interpolate the entity's position. */
	
	for (i = 0; i < 3; i++)
	{
		move[i] = entity->backlerp * move[i] + frontlerp * frame->translate[i];
	}

	for (i = 0; i < 3; i++)
	{
		frontv[i] = frontlerp * frame->scale[i];
		backv[i] = entity->backlerp * oldframe->scale[i];
	}

	/* Interpolate and transform the vertices. */
	
	lerp = pt_lerped[0];

	for (i = 0; i < alias_header->num_xyz; i++, v++, ov++, lerp += 4)
	{
		lerped_vertex[0] = move[0] + ov->v[0] * backv[0] + v->v[0] * frontv[0];
		lerped_vertex[1] = move[1] + ov->v[1] * backv[1] + v->v[1] * frontv[1];
		lerped_vertex[2] = move[2] + ov->v[2] * backv[2] + v->v[2] * frontv[2];
		lerped_vertex[3] = 1;
		
		/* Apply the transformation to this vertex and store the result. */
		
		for (j = 0; j < 3; ++j)
		{
			lerp[j] = 0;
			for (k = 0; k < 4; ++k)
			{
				lerp[j] += transformation_matrix[k * 4 + j] * lerped_vertex[k];
			}
		}
	}
	
	/* Create a leaf node for each triangle. */

	triangles = (dtriangle_t *)((byte *)alias_header + alias_header->ofs_tris);
	
	int first_node_index = pt_num_nodes;
	int num_added_nodes = 0;
	
	for (i = 0; i < alias_header->num_tris; ++i)
	{
		if (pt_num_nodes >= PT_MAX_TRI_NODES || pt_num_triangles >= PT_MAX_TRIANGLES)
			continue;
				
		tri = triangles + i;
		node = AllocateNode();
		
		num_added_nodes++;
		
		node->leaf = true;
		node->triangle_index = pt_num_triangles++;
		
		/* Get the bounding box of the triangles. */
		
		for (j = 0; j < 3; ++j)
		{
			lerp = pt_lerped[tri->index_xyz[j]];
			
			for (k = 0; k < 3; ++k)
			{
				if (node->aabb_min[k] > lerp[k])
					node->aabb_min[k] = lerp[k];

				if(node->aabb_max[k] < lerp[k])
					node->aabb_max[k] = lerp[k];
			}
		}
		
		node->morton_code = TriNodeCalculateMortonCode(node);
		node->surface_area = TriNodeCalculateSurfaceArea(node);
		
		pt_triangle_data[node->triangle_index * 2 + 0] = (int)tri->index_xyz[0] | ((int)tri->index_xyz[1] << 16);
		pt_triangle_data[node->triangle_index * 2 + 1] = (int)tri->index_xyz[2];
	}
	
	/* Sort the leaf nodes by morton code so that nodes which are spatially close are ordinally close. */
	
	qsort(pt_trinodes_ordered + first_node_index, num_added_nodes, sizeof(pt_trinodes_ordered[0]), TriNodeMortonCodeComparator);

}

static void
AddBrushModel(entity_t *entity, model_t *model)
{
}

static void
AddEntities(void)
{
	int i;
	entity_t *entity;
	model_t *model;
	
	for (i = 0; i < r_newrefdef.num_entities; i++)
	{
		entity = &r_newrefdef.entities[i];
		
		if (entity->flags & (RF_DEPTHHACK | RF_WEAPONMODEL | RF_TRANSLUCENT | RF_BEAM | RF_NOSHADOW | RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM))
			continue;
		
		model = entity->model;
		
		if (!model)
			continue;
		
		switch (model->type)
		{
			case mod_alias:
				AddAliasModel(entity, model);
				break;
			case mod_brush:
				AddBrushModel(entity, model);
				break;
			case mod_sprite:
				break;
			default:
				VID_Error(ERR_DROP, "Bad modeltype");
				break;
		}
	}
}

void
R_UpdatePathtracerForCurrentFrame(void)
{
	pt_num_nodes = 0;
	pt_num_triangles = 0;
	
	AddEntities();
}
	
static void
CreateTextureBuffer(GLuint *buffer, GLuint *texture, GLenum format, GLsizei size)
{
	*buffer = 0;
	*texture = 0;
	
	if (qglGenBuffersARB)
		qglGenBuffersARB(1, buffer);
	else if(qglGenBuffers)
		qglGenBuffers(1, buffer);
	
	glGenTextures(1, texture);
	
	if (qglBindBufferARB)
		qglBindBufferARB(GL_TEXTURE_BUFFER, *buffer);
	else if (qglBindBuffer)
		qglBindBuffer(GL_TEXTURE_BUFFER, *buffer);
	
	if (qglBufferDataARB)
		qglBufferDataARB(GL_TEXTURE_BUFFER, size, NULL, GL_DYNAMIC_DRAW_ARB);
	else if (qglBufferData)
		qglBufferData(GL_TEXTURE_BUFFER, size, NULL, GL_DYNAMIC_DRAW);

	glBindTexture(GL_TEXTURE_BUFFER, *texture);

	if (qglTexBufferARB)
		qglTexBufferARB(GL_TEXTURE_BUFFER, format, *buffer);
	else if (qglTexBufferEXT)
		qglTexBufferEXT(GL_TEXTURE_BUFFER, format, *buffer);
	else if (qglTexBuffer)
		qglTexBuffer(GL_TEXTURE_BUFFER, format, *buffer);
		
	if (qglBindBufferARB)
		qglBindBufferARB(GL_TEXTURE_BUFFER, 0);
	else if (qglBindBuffer)
		qglBindBuffer(GL_TEXTURE_BUFFER, 0);
	
	glBindTexture(GL_TEXTURE_BUFFER, 0);
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
	

	CreateTextureBuffer(&pt_node0_buffer, &pt_node0_texture, GL_RGBA32I, PT_MAX_TRI_NODES * 4 * sizeof(GLint));
	CreateTextureBuffer(&pt_node1_buffer, &pt_node1_texture, GL_RGBA32I, PT_MAX_TRI_NODES * 4 * sizeof(GLint));
	CreateTextureBuffer(&pt_triangle_buffer, &pt_triangle_texture, GL_RG32I, PT_MAX_TRIANGLES * 2 * sizeof(GLint));
	CreateTextureBuffer(&pt_vertex_buffer, &pt_vertex_texture, GL_RGB32F, PT_MAX_VERTICES * 3 * sizeof(GLfloat));
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

