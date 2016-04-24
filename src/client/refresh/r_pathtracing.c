#include "header/local.h"

#define PT_MAX_TRI_NODES		16384
#define PT_MAX_TRIANGLES		16384
#define PT_MAX_VERTICES			16384
#define PT_MAX_NODE_CHILDREN	4
#define PT_MAX_NODE_DEPTH		4
#define PT_MAX_TRI_LIGHTS		16384
#define PT_MAX_TRI_LIGHT_REFS	(1 << 20)
#define PT_MAX_CLUSTERS			16384

cvar_t *gl_pt_enable;
cvar_t *gl_pt_stats;

GLhandleARB pt_program_handle;

GLuint pt_node_texture = 0;
GLuint pt_child_texture = 0;
GLuint pt_bsp_lightref_texture = 0;

GLuint pt_node0_buffer = 0;
GLuint pt_node0_texture = 0;
GLuint pt_node1_buffer = 0;
GLuint pt_node1_texture = 0;
GLuint pt_triangle_buffer = 0;
GLuint pt_triangle_texture = 0;
GLuint pt_vertex_buffer = 0;
GLuint pt_vertex_texture = 0;
GLuint pt_trilights_buffer = 0;
GLuint pt_trilights_texture = 0;
GLuint pt_lightref_buffer = 0;
GLuint pt_lightref_texture = 0;

GLint pt_frame_counter_loc = -1;
GLint pt_entity_to_world_loc = -1;

static GLhandleARB vertex_shader;
static GLhandleARB fragment_shader;

static unsigned long int pt_bsp_texture_width = 0, pt_bsp_texture_height = 0;

static const GLcharARB* vertex_shader_source =
	"#version 120\n"
	"uniform mat4 entity_to_world = mat4(1);\n"
	"varying vec4 texcoords[5];\n"
	"void main()\n"
	"{\n"
	"	gl_Position = ftransform();\n"
	"	texcoords[0] = gl_MultiTexCoord0;\n"
	"	texcoords[1] = entity_to_world * gl_Vertex;\n"
	"	texcoords[2].xyz = vec3(0.0, 0.0, 0.0);\n"
	"	texcoords[3].xyz = mat3(entity_to_world) * gl_MultiTexCoord2.xyz;\n"
	"	texcoords[4] = gl_MultiTexCoord3;\n"
	"}\n"
	"\n";

static const GLcharARB* fragment_shader_source =
	"#version 330\n"
	"\n"
	"#define EPS	 (1./32.)\n"
	"#define MAXT	(2048.)\n"
	"\n"
	"in vec4 texcoords[5];\n"
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
	"vec3 rp = texcoords[1].xyz;\n"
	"vec3 dir = normalize(texcoords[2].xyz);\n"
	"vec3 normal = texcoords[3].xyz;\n"
	"vec4 out_pln;\n"
	"\n"
	"\n"
	"\n"
	"uniform isamplerBuffer node0;\n"
	"uniform isamplerBuffer node1;\n"
	"uniform samplerBuffer edge0;\n"
	"uniform isamplerBuffer triangle;\n"
	"\n"
	"uniform samplerBuffer lights;\n"
	"uniform isamplerBuffer lightrefs;\n"
	"uniform isampler2D bsp_lightrefs;\n"
	"\n"
	"vec2 boxInterval(vec3 ro, vec3 rd, vec3 size)\n"
	"{\n"
	"	vec3 mins = (size * -sign(rd) - ro) / rd;\n"
	"	vec3 maxs = (size * +sign(rd) - ro) / rd;	\n"
	"\n"
	"	return vec2(max(max(mins.x, mins.y), mins.z), min(min(maxs.x, maxs.y), maxs.z));\n"
	"}\n"
	"\n"
	"bool traceRayShadowTri(vec3 ro, vec3 rd, float maxdist)\n"
	"{\n"
	"	int node = 0;\n"
	"\n"
	"	do\n"
	"	{\n"
	"		ivec4 n0 = texelFetch(node0, node);\n"
	"		ivec4 n1 = texelFetch(node1, node);\n"
	"\n"
	"		vec2 i = boxInterval(ro - intBitsToFloat(n1.xyz), rd, intBitsToFloat(n0.xyz));\n"
	"\n"
	"		if (i.x < i.y && i.x < maxdist && i.y > 0.0)\n"
	"		{\n"
	"			if (n1.w != -1)\n"
	"			{\n"
	"				ivec2 tri = texelFetch(triangle, n1.w).xy;\n"
	"\n"
	"				vec3 p0 = texelFetch(edge0, tri.x & 0xffff).xyz;\n"
	"				vec3 p1 = texelFetch(edge0, tri.x >> 16).xyz;\n"
	"				vec3 p2 = texelFetch(edge0, tri.y).xyz;\n"
	"\n"
	"				vec3 n = cross(p1 - p0, p2 - p0);\n"
	"				float t = dot(p0 - ro, n) / dot(rd, n);\n"
	"\n"
	"				float d0 = dot(rd, cross(p0 - ro, p1 - ro));\n"
	"				float d1 = dot(rd, cross(p1 - ro, p2 - ro));\n"
	"				float d2 = dot(rd, cross(p2 - ro, p0 - ro));\n"
	"\n"
	"				if (sign(d0) == sign(d1) && sign(d0) == sign(d2) && t < maxdist && t > 0.0)\n"
	"					return false;\n"
	"			}\n"
	"			\n"
	"			++node;\n"
	"		}\n"
	"		else\n"
	"			node = n0.w;\n"
	"\n"
	"	} while(node > 0);\n"
	"\n"
	"	return true;\n"
	"}\n"
	"bool traceRayShadowBSP(vec3 org, vec3 dir, float t0, float max_t)\n"
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
	"			vec4 pln=texture(planes,node);\n"
	"			vec4 children=texture(branches,node);\n"
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
	"int getLightRef(vec3 p)\n"
	"{\n"
	" int index=0;\n"
	"	vec2  node=vec2(0);\n"
	"		do{\n"
	"			vec4 pln=texture(planes,node);\n"
	"			vec4 children=texture(branches,node);\n"
	"			ivec2 light_indices = texture(bsp_lightrefs, node).rg;\n"
	"			\n"
	"			float d=dot(pln.xyz,p)-pln.w;\n"
	"			if(d<0.0) { node = children.zw; index = light_indices.y; } else { node = children.xy; index = light_indices.x; };\n"
	"\n"
	"			if(node.y==1.0)\n"
	"			{\n"
	"				 return -1;\n"
	"			}\n"
	"			\n"
	"		} while(node!=vec2(0.0));\n"
	"	return index;\n"
	"}\n"
	"\n"
	"void main()\n"
	"{\n"
	"	seed = gl_FragCoord.x * 89.9 + gl_FragCoord.y * 197.3 + frame*0.02;\n"
	"\n"
	"	rp+=normal*EPS*16;\n"
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
	"	vec3 r=texcoords[4].rgb;\n"
	"	  \n"
	" int oli=getLightRef(rp),li=oli;\n"
	"	int ref=texelFetch(lightrefs, li).r;\n"
	"  float wsum=0.;\n"
	" if(ref != -1) do{\n"
	" 				vec4 light=texelFetch(lights, ref);\n"
	"				ivec2 tri = texelFetch(triangle, floatBitsToInt(light.w)).xy;\n"
	"				vec3 p0 = texelFetch(edge0, tri.x & 0xffff).xyz;\n"
	"				vec3 p1 = texelFetch(edge0, tri.x >> 16).xyz;\n"
	"				vec3 p2 = texelFetch(edge0, tri.y).xyz;\n"
	"				vec3 n = cross(p2 - p0, p1 - p0);\n"
	"				float d=distance(rp,(p0+p1+p2)/3.);\n"
   "   			float pd=dot(rp-p0,n);\n"
   "   			float w=1./(d*d)*max(0.,pd);\n"
   "   			wsum+=w;\n"
	"				++li;\n"
	"				ref=texelFetch(lightrefs, li).r;\n"
	" } while(ref!=-1);\n"
	"\n"
	"{\n"
   "   float x=rand()*wsum,w=1;\n"
   "   vec4 j=vec4(0);\n"
   "\n"   
	" li=oli;\n"
	"	int ref=texelFetch(lightrefs, li).r;\n"
	" 	if(ref != -1) 	do{\n"
	" 				vec4 light=texelFetch(lights, ref);\n"
	"				ivec2 tri = texelFetch(triangle, floatBitsToInt(light.w)).xy;\n"
	"				vec3 p0 = texelFetch(edge0, tri.x & 0xffff).xyz;\n"
	"				vec3 p1 = texelFetch(edge0, tri.x >> 16).xyz;\n"
	"				vec3 p2 = texelFetch(edge0, tri.y).xyz;\n"
	"				vec3 n = cross(p2 - p0, p1 - p0);\n"
	"				float d=distance(rp,(p0+p1+p2)/3.);\n"
   "   			float pd=dot(rp-p0,n);\n"
   "   			float w=1./(d*d)*max(0.,pd);\n"	
   "      x-=w;\n"
   "      if(x<0.)\n"
   "      {\n"
   "         j=light;\n"
   "         break;\n"
   "      }\n"
	"				++li;\n"
	"				ref=texelFetch(lightrefs, li).r;\n"
	" 		} while(ref!=-1);\n"
	
	"	vec4 light=j;\n"
	"				ivec2 tri = texelFetch(triangle, floatBitsToInt(light.w)).xy;\n"
	"				vec3 p0 = texelFetch(edge0, tri.x & 0xffff).xyz;\n"
	"				vec3 p1 = texelFetch(edge0, tri.x >> 16).xyz;\n"
	"				vec3 p2 = texelFetch(edge0, tri.y).xyz;\n"
	"				vec3 n = cross(p2 - p0, p1 - p0);\n"
	"				float d=distance(rp,(p0+p1+p2)/3.);\n"
   "   			float pd=dot(rp-p0,n);\n"
   "   			w=1./(d*d)*max(0.,pd);\n"	
   "   vec3 sp=rp;\n"
   "   vec3 sn=spln.xyz;\n"
   "   vec3 lp=p0;\n"
	"	 vec2 uv = vec2(rand(), rand());\n"
	"	 if((uv.x + uv.y) > 1.) uv = vec2(1) - uv;\n"
   "   lp+=(p1 - p0)*uv.x+(p2 - p0)*uv.y;\n"
   "   float ld=distance(sp,lp);\n"
   "   vec3 l=(lp-sp)/ld;\n"
   "   float ndotl=dot(l,sn),lndotl=dot(-l,n);\n"
   "   if(ndotl>0. && lndotl>0.)\n"
   "   {\n"
   "   	float s=(traceRayShadowBSP(sp,l,EPS*16,min(2048.,ld)) && traceRayShadowTri(sp,l,min(2048.,ld))) ? 1./(ld*ld) : 0.;\n"
   "   	r+=s*ndotl*lndotl*light.rgb/(w/wsum);\n"
	"	}\n"
	"}\n"
	"	gl_FragColor.rgb = r / 1024.;\n"

	/*	This code implements ambient occlusion. It has been temporarily disabled.
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
	"			if(traceRayShadowBSP(rp,rd,EPS*16,aoradius) && traceRayShadowTri(rp,rd,aoradius))\n"
	"				r+=vec3(1.0/float(n));\n"
	"	}\n"
	"\n"
	"	gl_FragColor.rgb = r;\n"
	"	gl_FragColor.a = 1.0;\n"
	*/
	
	"	gl_FragColor.a = 1.0;\n"
	"	gl_FragColor.rgb *= texture(tex0, texcoords[0].st).rgb + vec3(1e-2);\n"
	"	gl_FragColor.rgb = sqrt(gl_FragColor.rgb);\n"
	"}\n"
	"\n";

	
typedef float vec4_t[4];
static vec4_t pt_lerped[MAX_VERTS];

typedef struct trilight_s
{
	short triangle_index;
	qboolean quad;
	float radiant_flux[3];
	msurface_t* surface;
} trilight_t;

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
static trilight_t pt_trilights[PT_MAX_TRI_LIGHTS];
static int pt_trilight_references[PT_MAX_TRI_LIGHT_REFS];
static int pt_cluster_light_references[PT_MAX_CLUSTERS];

static short pt_num_nodes = 0;
static short pt_num_triangles = 0;
static short pt_num_vertices = 0;
static short pt_written_nodes = 0;
static short pt_previous_node = -1;
static short pt_num_lights = 0;
static int pt_num_trilight_references = 0;

static short pt_dynamic_vertices_offset = 0;
static short pt_dynamic_triangles_offset = 0;

static int pt_triangle_data[PT_MAX_TRIANGLES * 2];
static int pt_node0_data[PT_MAX_TRI_NODES * 4];
static int pt_node1_data[PT_MAX_TRI_NODES * 4];
static float pt_vertex_data[PT_MAX_VERTICES * 3];
static float pt_trilight_data[PT_MAX_TRI_LIGHTS * 4];

static int
FloatBitsToInt(float x)
{
	return *(int*)&x;
}

static float
IntBitsToFloat(int x)
{
	return *(float*)&x;
}

static void
AddStaticLights(void)
{
	msurface_t *surf;
	mtexinfo_t *texinfo;
	trilight_t *light;
	int i, j, k, m;
	glpoly_t *p;
	float *v;
	int poly_offset;
	int light_index;
	mleaf_t *leaf, *other_leaf;
	byte *vis;
	int cluster;
					
	pt_num_lights = 0;

	/* Visit each surface in the worldmodel. */
	
	for (i = 0; i < r_worldmodel->numsurfaces; ++i)
	{
		surf = r_worldmodel->surfaces + i;
		texinfo = surf->texinfo;
		if ((texinfo->flags & SURF_LIGHT) && texinfo->radiance > 0)
		{			
			p = surf->polys;
			
			v = p->verts[0];
			
			poly_offset = pt_num_vertices;
		
			for (k = 0; k < p->numverts; k++, v += VERTEXSIZE)
			{
				/* Store this vertex of the polygon. */
				
				for (j = 0; j < 3; ++j)
					pt_vertex_data[pt_num_vertices * 3 + j] = v[j];

				pt_num_vertices++;

				if (k > 1)
				{
					/* Add a new triangle light for this segment of the polygon. */
					if (pt_num_lights >= PT_MAX_TRI_LIGHTS)
						continue;
					
					int ind[3] = { poly_offset, poly_offset + k - 1, poly_offset + k };

					light_index = pt_num_lights++;
					light = pt_trilights + light_index;
					
					light->quad = false;
					light->triangle_index = pt_num_triangles++;
					light->surface = surf;
					
					/* Calculate the radiant flux of the light. */
					
					for (j = 0; j < 3; ++j)
						light->radiant_flux[j] = texinfo->image->reflectivity[j] * texinfo->radiance;
										
					pt_triangle_data[light->triangle_index * 2 + 0] = ind[0] | (ind[1] << 16);
					pt_triangle_data[light->triangle_index * 2 + 1] = ind[2];
					
					/* Pack the data into the buffer. */

					pt_trilight_data[light_index * 4 + 0] = light->radiant_flux[0];
					pt_trilight_data[light_index * 4 + 1] = light->radiant_flux[1];
					pt_trilight_data[light_index * 4 + 2] = light->radiant_flux[2];
					pt_trilight_data[light_index * 4 + 3] = IntBitsToFloat(light->triangle_index);
				}
			}			
		}
	}
	
	/* Reset the cluster light reference lists. */
	
	for (i = 0; i < PT_MAX_CLUSTERS; ++i)
		pt_cluster_light_references[i] = -1;
	
	/* Visit each leaf in the worldmodel and build the list of visible lights for each cluster. */
		
	for (i = 0; i < r_worldmodel->numleafs; ++i)
	{
		if (pt_num_trilight_references >= PT_MAX_TRI_LIGHT_REFS)
			continue;
	
		leaf = r_worldmodel->leafs + i;
		
		if (leaf->contents == CONTENTS_SOLID || leaf->cluster == -1)
			continue;
		
		/* Skip clusters which have already been processed. */

		if (pt_cluster_light_references[leaf->cluster] != -1)
			continue;
		
		pt_cluster_light_references[leaf->cluster] = pt_num_trilight_references;
		
		/* Get the PVS bits for this cluster. */
		
		vis = Mod_ClusterPVS(leaf->cluster, r_worldmodel);

		/* Go through every visible leaf and build a list of the lights which have corresponding
			surfaces in that leaf. */
		
		for (j = 0; j < r_worldmodel->numleafs; ++j)
		{
			other_leaf = r_worldmodel->leafs + j;
			
			if (other_leaf->contents == CONTENTS_SOLID)
				continue;
		
			cluster = other_leaf->cluster;

			if (cluster == -1)
				continue;

			if (vis[cluster >> 3] & (1 << (cluster & 7)))
			{
				/* This leaf is visible, so look for any light-emitting surfaces within it. */
				
				for (k = 0; k < other_leaf->nummarksurfaces; ++k)
				{
					surf = other_leaf->firstmarksurface[k];
					
					for (m = 0; m < pt_num_lights; ++m)
					{
						if (pt_trilights[m].surface == surf)
						{
							if (pt_num_trilight_references < PT_MAX_TRI_LIGHT_REFS)
							{
								/* A light-emitting surface has been found, so add a reference to the light into the reference list. */
								pt_trilight_references[pt_num_trilight_references++] = m;
							}
						}
					}
				}
			}
		}
		
		if (pt_num_trilight_references < PT_MAX_TRI_LIGHT_REFS)
		{
			/* Add the end-of-list marker. */
			pt_trilight_references[pt_num_trilight_references++] = -1;
		}
	}
}

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
TriNodeWriteData(const trinode_t *n, int index)
{
	int i, c, prev_child_index;
	int child_index, m;
	int *n0 = pt_node0_data + index * 4;
	int *n1 = pt_node1_data + index * 4;
	int *pn0;
	float aabb_size[3];

	for (i = 0; i < 3; ++i)
		aabb_size[i] = n->aabb_max[i] - n->aabb_min[i];

	/* Completely degenerate nodes can be skipped. */
	if (aabb_size[0] < 1e-3f && aabb_size[1] < 1e-3f && aabb_size[2] < 1e-3f)
		return 0;
	
	for (i = 0; i < 3; ++i)
	{
		/* A small expansion of the bounding box is made here, so that axial triangles don't result in a degenerate bounding box. */
		n0[i] = FloatBitsToInt(aabb_size[i] / 2.0f + 1e-3f);
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
		m = TriNodeWriteData(n->children[i], child_index);

		if (m > 0)
		{
			if (prev_child_index != -1)
			{
				pn0 = pt_node0_data + prev_child_index * 4;
				pn0[3] = child_index;
			}

			prev_child_index = child_index;
			c += m;
		}
	}

	if (prev_child_index != -1)
	{
		pn0 = pt_node0_data + prev_child_index * 4;
		pn0[3] = index + c;
	}
	
	return c;
}

static void
WriteTriNodes(int first_node_index, int num_nodes)
{
	int i, m;
	int *pn0;
	
	for (i = 0; i < num_nodes; ++i)
	{
		m = TriNodeWriteData(pt_trinodes_ordered[first_node_index + i], pt_written_nodes);
		if (m > 0)
		{
			if (pt_previous_node >= 0)
			{
				pn0 = pt_node0_data + pt_previous_node * 4;
				pn0[3] = pt_written_nodes;
			}
			pt_previous_node = pt_written_nodes;
			pt_written_nodes += m;
		}
	}
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
	DeleteTexture(&pt_bsp_lightref_texture);

	DeleteBuffer(&pt_node0_buffer);
	DeleteTexture(&pt_node0_texture);
	DeleteBuffer(&pt_node1_buffer);
	DeleteTexture(&pt_node1_texture);
	DeleteBuffer(&pt_triangle_buffer);
	DeleteTexture(&pt_triangle_texture);
	DeleteBuffer(&pt_vertex_buffer);
	DeleteTexture(&pt_vertex_texture);
	DeleteBuffer(&pt_trilights_buffer);
	DeleteTexture(&pt_trilights_texture);
	DeleteBuffer(&pt_lightref_buffer);
	DeleteTexture(&pt_lightref_texture);
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

/* Constructs a transformation matrix to match the one used for drawing entities. This is based on the GL matrix transformation
	code in R_DrawAliasModel and R_RotateForEntity. */
void
R_ConstructEntityToWorldMatrix(float m[16], entity_t *entity)
{
	MatrixIdentity(m);	
	MatrixTranslate(m, entity->origin[0], entity->origin[1], entity->origin[2]);

	entity->angles[PITCH] = -entity->angles[PITCH];
	MatrixRotateAxis(m, 2, entity->angles[1] * M_PI / 180.0);
	MatrixRotateAxis(m, 1, -entity->angles[0] * M_PI / 180.0);
	MatrixRotateAxis(m, 0, -entity->angles[2] * M_PI / 180.0);
	entity->angles[PITCH] = -entity->angles[PITCH];
}

static void
BuildAndWriteEntityNodesHierarchy(int first_node_index, int num_added_nodes, float entity_aabb_min[3], float entity_aabb_max[3])
{
	int first_node_index2, num_added_nodes2, i, j, k, m;
	int *entitynode_n0, *entitynode_n1;
	trinode_t *node;
	
	/* Sort the leaf nodes by morton code so that nodes which are spatially close are ordinally close. */
	
	qsort(pt_trinodes_ordered + first_node_index, num_added_nodes, sizeof(pt_trinodes_ordered[0]), TriNodeMortonCodeComparator);

	/* Group nodes together to create a hierarchy in bottom-up style. */
	
	for (i = 0; i < PT_MAX_NODE_DEPTH; ++i)
	{
		first_node_index2 = pt_num_nodes;
		num_added_nodes2 = 0;
		for (j = 0; j < num_added_nodes; j += PT_MAX_NODE_CHILDREN)
		{
			if (pt_num_nodes >= PT_MAX_TRI_NODES)
				continue;
			
			node = AllocateNode();
			
			num_added_nodes2++;
			
			node->leaf = false;
			
			for (k = 0; k < PT_MAX_NODE_CHILDREN && (j + k) < num_added_nodes; ++k)
			{
				node->children[k] = pt_trinodes_ordered[first_node_index + j + k];
				for (m = 0; m < 3; ++m)
				{
					if(node->aabb_min[m] > node->children[k]->aabb_min[m])
						node->aabb_min[m] = node->children[k]->aabb_min[m];

					if(node->aabb_max[m] < node->children[k]->aabb_max[m])
						node->aabb_max[m] = node->children[k]->aabb_max[m];
				}
				++node->num_children;
			}
			
			node->surface_area = TriNodeCalculateSurfaceArea(node);
		}
		first_node_index = first_node_index2;
		num_added_nodes = num_added_nodes2;
	}
	
	/* Sort the top-level nodes by surface area so that the nodes with largest area are visited first. This is done because the
		larger nodes are more likely to be intersected by a given random ray. */
	
	qsort(pt_trinodes_ordered + first_node_index, num_added_nodes, sizeof(pt_trinodes_ordered[0]), TriNodeSurfaceAreaComparator);

	/* Make one node for the whole entity, so it can be skipped entirely with a single bounding box test. */

	entitynode_n0 = pt_node0_data + pt_written_nodes * 4;
	entitynode_n1 = pt_node1_data + pt_written_nodes * 4;
	
	for (i = 0; i < 3; ++i)
	{
		entitynode_n0[i] = FloatBitsToInt((entity_aabb_max[i] - entity_aabb_min[i]) / 2.0f);
		entitynode_n1[i] = FloatBitsToInt((entity_aabb_min[i] + entity_aabb_max[i]) / 2.0f);
	}

	entitynode_n1[3] = -1;
	
	++pt_written_nodes;
	
	WriteTriNodes(first_node_index, num_added_nodes);
	
	entitynode_n0[3] = pt_written_nodes;
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
	int first_node_index, num_added_nodes;
	int triangle_vertices_offset;
	float entity_aabb_min[3], entity_aabb_max[3];
	
	/* Get the entity-to-world transformation matrix. */
	
	R_ConstructEntityToWorldMatrix(transformation_matrix, entity);
	
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

	/* Initialise the entity's bounding box. */
	
	for (i = 0; i < 3; ++i)
	{
		entity_aabb_min[i] = 1e9f;
		entity_aabb_max[i] = -1e9f;
	}

	/* Interpolate and transform the vertices. */
	
	lerp = pt_lerped[0];

	triangle_vertices_offset = pt_num_vertices;
	
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
			
			pt_vertex_data[pt_num_vertices * 3 + j] = lerp[j];
			
			/* If necessary, expand the entity's bounding box to include this vertex. */
			
			if (entity_aabb_min[j] > lerp[j])
				entity_aabb_min[j] = lerp[j];

			if (entity_aabb_max[j] < lerp[j])
				entity_aabb_max[j] = lerp[j];
		}
		
		pt_num_vertices++;
	}
	
	/* Create a leaf node for each triangle. */

	triangles = (dtriangle_t *)((byte *)alias_header + alias_header->ofs_tris);
		
	first_node_index = pt_num_nodes;
	num_added_nodes = 0;
	
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
		
		pt_triangle_data[node->triangle_index * 2 + 0] = (triangle_vertices_offset + (int)tri->index_xyz[0]) | ((triangle_vertices_offset + (int)tri->index_xyz[1]) << 16);
		pt_triangle_data[node->triangle_index * 2 + 1] = (triangle_vertices_offset + (int)tri->index_xyz[2]);
	}
	
	BuildAndWriteEntityNodesHierarchy(first_node_index, num_added_nodes, entity_aabb_min, entity_aabb_max);
}

static void
AddBrushModel(entity_t *entity, model_t *model)
{
	float transformation_matrix[16];
	trinode_t *node;
	int first_node_index, num_added_nodes;
	int poly_offset;
	float entity_aabb_min[3], entity_aabb_max[3];
	int i, j, k, m;
	msurface_t *psurf;
	float *v, x;
	vec4_t vertex;
	glpoly_t *p;
	
	if (model->nummodelsurfaces == 0)
		return;

	/* Get the entity-to-world transformation matrix. */
	
	entity->angles[2] = -entity->angles[2];
	R_ConstructEntityToWorldMatrix(transformation_matrix, entity);
	entity->angles[2] = -entity->angles[2];	

	/* Initialise the entity's bounding box. */
	
	for (i = 0; i < 3; ++i)
	{
		entity_aabb_min[i] = 1e9f;
		entity_aabb_max[i] = -1e9f;
	}
	
	psurf = &model->surfaces[model->firstmodelsurface];

	first_node_index = pt_num_nodes;
	num_added_nodes = 0;
	
	for (i = 0; i < model->nummodelsurfaces; i++, psurf++)
	{
		if (psurf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66))
			continue;

		p = psurf->polys;

		v = p->verts[0];

		poly_offset = pt_num_vertices;
		
		for (k = 0; k < p->numverts; k++, v += VERTEXSIZE)
		{
			/* Apply the transformation to this vertex and store the result. */
			
			for (j = 0; j < 3; ++j)
			{
				vertex[j] = 0;

				vertex[j] += transformation_matrix[0 * 4 + j] * v[0];
				vertex[j] += transformation_matrix[1 * 4 + j] * v[1];
				vertex[j] += transformation_matrix[2 * 4 + j] * v[2];
				vertex[j] += transformation_matrix[3 * 4 + j];
				
				pt_vertex_data[pt_num_vertices * 3 + j] = vertex[j];
				
				/* If necessary, expand the entity's bounding box to include this vertex. */
				
				if (entity_aabb_min[j] > vertex[j])
					entity_aabb_min[j] = vertex[j];

				if (entity_aabb_max[j] < vertex[j])
					entity_aabb_max[j] = vertex[j];
			}
			
			pt_num_vertices++;

			if (k > 1)
			{
				/* Add a new triangle and leaf node for this segment of the polygon. */
				if (pt_num_nodes >= PT_MAX_TRI_NODES || pt_num_triangles >= PT_MAX_TRIANGLES)
					continue;
				
				int ind[3] = { poly_offset, poly_offset + k - 1, poly_offset + k };

				node = AllocateNode();
				
				num_added_nodes++;
				
				node->leaf = true;
				node->triangle_index = pt_num_triangles++;
				
				/* Get the bounding box of the triangle. */
				
				for (j = 0; j < 3; ++j)
				{					
					for (m = 0; m < 3; ++m)
					{
						x = pt_vertex_data[ind[j] * 3 + m];

						if (node->aabb_min[m] > x)
							node->aabb_min[m] = x;

						if(node->aabb_max[m] < x)
							node->aabb_max[m] = x;
					}
				}
				
				node->morton_code = TriNodeCalculateMortonCode(node);
				node->surface_area = TriNodeCalculateSurfaceArea(node);
				
				pt_triangle_data[node->triangle_index * 2 + 0] = ind[0] | (ind[1] << 16);
				pt_triangle_data[node->triangle_index * 2 + 1] = ind[2];
			}				
		}

	}
	
	BuildAndWriteEntityNodesHierarchy(first_node_index, num_added_nodes, entity_aabb_min, entity_aabb_max);
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
		
		if (entity->flags & (RF_FULLBRIGHT | RF_DEPTHHACK | RF_WEAPONMODEL | RF_TRANSLUCENT | RF_BEAM | RF_NOSHADOW | RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM))
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

static void
UploadTextureBufferData(GLuint buffer, void *data, GLsizei size)
{
	if (qglBindBufferARB)
		qglBindBufferARB(GL_TEXTURE_BUFFER, buffer);
	else if (qglBindBuffer)
		qglBindBuffer(GL_TEXTURE_BUFFER, buffer);
	
	if (qglBufferSubDataARB)	
		qglBufferSubDataARB(GL_TEXTURE_BUFFER, 0, size, data);
	else
		qglBufferSubData(GL_TEXTURE_BUFFER, 0, size, data);
	
	if (qglBindBufferARB)
		qglBindBufferARB(GL_TEXTURE_BUFFER, 0);
	else if (qglBindBuffer)
		qglBindBuffer(GL_TEXTURE_BUFFER, 0);
}

void
R_UpdatePathtracerForCurrentFrame(void)
{
	int i;
	
	pt_num_nodes = 0;
	pt_num_triangles = pt_dynamic_triangles_offset;
	pt_num_vertices = pt_dynamic_vertices_offset;
	pt_written_nodes = 0;
	pt_previous_node = -1;

	AddEntities();
	
	/* Nullify the entry just past the last node, so that we can guarantee that traversal terminates correctly. */
	
	for (i = 0; i < 4; ++i)
	{
		pt_node0_data[pt_written_nodes * 4 + i] = 0;
		pt_node1_data[pt_written_nodes * 4 + i] = 0;
	}

	pt_written_nodes++;
		
	UploadTextureBufferData(pt_node0_buffer, pt_node0_data, pt_written_nodes * 4 * sizeof(pt_node0_data[0]));
	UploadTextureBufferData(pt_node1_buffer, pt_node1_data, pt_written_nodes * 4 * sizeof(pt_node1_data[0]));
	UploadTextureBufferData(pt_triangle_buffer, pt_triangle_data, pt_num_triangles * 2 * sizeof(pt_triangle_data[0]));
	UploadTextureBufferData(pt_vertex_buffer, pt_vertex_data, pt_num_vertices * 3 * sizeof(pt_vertex_data[0]));

	if (gl_pt_stats->value)
		VID_Printf(PRINT_ALL, "pt_stats: n=%5d, t=%5d, v=%5d, w=%5d\n", pt_num_nodes, pt_num_triangles, pt_num_vertices, pt_written_nodes);
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

static void
AddStaticBSP()
{
	unsigned long int num_texels;
	float *tex_node_data;
	unsigned char *tex_child_data;
	int *tex_light_data;
	unsigned int i, j;
	mnode_t *in;
	mleaf_t *leaf;
	int cluster;
	
	pt_bsp_texture_width = 1;
	pt_bsp_texture_height = 1;
	
	num_texels = r_worldmodel->numnodes;

	while (pt_bsp_texture_width * pt_bsp_texture_height < num_texels)
	{
		pt_bsp_texture_width <<= 1;
		
		if (pt_bsp_texture_width * pt_bsp_texture_height >= num_texels)
			break;
		
		pt_bsp_texture_height <<= 1;
	}
	
	num_texels = pt_bsp_texture_width * pt_bsp_texture_height;
	
	tex_node_data = (float*)Z_Malloc(num_texels * 4 * sizeof(float));
	tex_child_data = (unsigned char*)Z_Malloc(num_texels * 4);
	tex_light_data = (int*)Z_Malloc(num_texels * 2 * sizeof(int));
	
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
				tex_child_data[i * 4 + 0 + j * 2] = ((in->children[j] - (r_worldmodel->nodes + r_worldmodel->firstnode)) % pt_bsp_texture_width) * 256 / pt_bsp_texture_width;
				tex_child_data[i * 4 + 1 + j * 2] = ((in->children[j] - (r_worldmodel->nodes + r_worldmodel->firstnode)) / pt_bsp_texture_width) * 256 / pt_bsp_texture_height;
			}
			else
			{
				if (in->children[j]->contents == CONTENTS_SOLID)
				{
					/* The leaf is solid, so mark the reference as such. */
					tex_child_data[i * 4 + 0 + j * 2] = tex_child_data[i * 4 + 1 + j * 2] = 255;
				}
				else
				{
					/* The leaf is empty, so it may have a visible light list. */
					tex_child_data[i * 4 + 0 + j * 2] = tex_child_data[i * 4 + 1 + j * 2] = 0;
					
					leaf = (mleaf_t*)(in->children[j]);
					cluster = leaf->cluster;
					
					tex_light_data[i * 2 + j] = pt_cluster_light_references[cluster];
				}
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
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, pt_bsp_texture_width, pt_bsp_texture_height, 0, GL_RGBA, GL_FLOAT, tex_node_data);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenTextures(1, &pt_child_texture);
	glBindTexture(GL_TEXTURE_2D, pt_child_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pt_bsp_texture_width, pt_bsp_texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_child_data);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	glGenTextures(1, &pt_bsp_lightref_texture);
	glBindTexture(GL_TEXTURE_2D, pt_bsp_lightref_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32I, pt_bsp_texture_width, pt_bsp_texture_height, 0, GL_RG_INTEGER, GL_INT, tex_light_data);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	Z_Free(tex_node_data);
	Z_Free(tex_child_data);
	Z_Free(tex_light_data);
}

void
R_PreparePathtracer(void)
{
	FreeModelData();

	if (r_worldmodel == NULL)
	{
		VID_Printf(PRINT_ALL, "R_PreparePathtracer: r_worldmodel is NULL!\n");
		return;
	}
	
	if (r_worldmodel->numnodes == 0)
	{
		VID_Printf(PRINT_ALL, "R_PreparePathtracer: r_worldmodel->numnodes is zero!\n");
		return;
	}
	
	CreateTextureBuffer(&pt_node0_buffer, &pt_node0_texture, GL_RGBA32I, PT_MAX_TRI_NODES * 4 * sizeof(GLint));
	CreateTextureBuffer(&pt_node1_buffer, &pt_node1_texture, GL_RGBA32I, PT_MAX_TRI_NODES * 4 * sizeof(GLint));
	CreateTextureBuffer(&pt_triangle_buffer, &pt_triangle_texture, GL_RG32I, PT_MAX_TRIANGLES * 2 * sizeof(GLint));
	CreateTextureBuffer(&pt_vertex_buffer, &pt_vertex_texture, GL_RGB32F, PT_MAX_VERTICES * 3 * sizeof(GLfloat));
	CreateTextureBuffer(&pt_trilights_buffer, &pt_trilights_texture, GL_RGBA32F, PT_MAX_TRI_LIGHTS * 4 * sizeof(GLfloat));
	CreateTextureBuffer(&pt_lightref_buffer, &pt_lightref_texture, GL_R32I, PT_MAX_TRI_LIGHT_REFS * sizeof(GLint));
	
	pt_num_nodes = 0;
	pt_num_triangles = 0;
	pt_num_vertices = 0;
	pt_written_nodes = 0;
	pt_previous_node = -1;
	pt_num_lights = 0;
	pt_num_trilight_references = 0;

	AddStaticLights();

	VID_Printf(PRINT_DEVELOPER, "R_PreparePathtracer: %d static lights\n", pt_num_lights);

	AddStaticBSP();

	VID_Printf(PRINT_DEVELOPER, "R_PreparePathtracer: Static BSP texture size is %dx%d\n", pt_bsp_texture_width, pt_bsp_texture_height);
	
	UploadTextureBufferData(pt_trilights_buffer, pt_trilight_data, pt_num_lights * 4 * sizeof(pt_trilight_data[0]));
	UploadTextureBufferData(pt_lightref_buffer, pt_trilight_references, pt_num_trilight_references * sizeof(pt_trilight_references[0]));
	
	pt_dynamic_vertices_offset = pt_num_vertices;
	pt_dynamic_triangles_offset = pt_num_triangles;
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
	gl_pt_stats = Cvar_Get( "gl_pt_stats", "0", CVAR_ARCHIVE);

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
	qglUniform1iARB(qglGetUniformLocationARB(pt_program_handle, "node0"), 4);
	qglUniform1iARB(qglGetUniformLocationARB(pt_program_handle, "node1"), 5);
	qglUniform1iARB(qglGetUniformLocationARB(pt_program_handle, "edge0"), 6);
	qglUniform1iARB(qglGetUniformLocationARB(pt_program_handle, "triangle"), 7);
	qglUniform1iARB(qglGetUniformLocationARB(pt_program_handle, "lights"), 8);
	qglUniform1iARB(qglGetUniformLocationARB(pt_program_handle, "lightrefs"), 9);
	qglUniform1iARB(qglGetUniformLocationARB(pt_program_handle, "bsp_lightrefs"), 10);
	
	pt_frame_counter_loc = qglGetUniformLocationARB(pt_program_handle, "frame");
	pt_entity_to_world_loc = qglGetUniformLocationARB(pt_program_handle, "entity_to_world");
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

