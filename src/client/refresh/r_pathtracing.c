#include "header/local.h"

#define PT_MAX_TRI_NODES					8192
#define PT_MAX_TRIANGLES					8192
#define PT_MAX_VERTICES						8192
#define PT_MAX_NODE_CHILDREN				4
#define PT_MAX_NODE_DEPTH					4
#define PT_MAX_TRI_LIGHTS					8192
#define PT_MAX_TRI_LIGHT_REFS				(1 << 20)
#define PT_MAX_CLUSTERS						8192
#define PT_MAX_ENTITY_LIGHTS				2048
#define PT_MAX_ENTITY_LIGHT_CLUSTERS	8
#define PT_MAX_BSP_TREE_DEPTH				32
#define PT_RAND_TEXTURE_SIZE 				64
#define PT_MAX_CLUSTER_DLIGHTS			16
#define PT_NUM_ENTITY_TRILIGHTS			4
#define PT_NUM_ENTITY_VERTICES			4

#define MAX(a, b) ((a) > (b) ? (a) : (b))

cvar_t *gl_pt_enable;
cvar_t *gl_pt_stats;

static cvar_t *gl_pt_bounces;
static cvar_t *gl_pt_shadow_samples;
static cvar_t *gl_pt_light_samples;
static cvar_t *gl_pt_sky_enable;
static cvar_t *gl_pt_sky_samples;
static cvar_t *gl_pt_ao_enable;
static cvar_t *gl_pt_ao_radius;
static cvar_t *gl_pt_ao_color;
static cvar_t *gl_pt_ao_samples;
static cvar_t *gl_pt_translucent_surfaces_enable;
static cvar_t *gl_pt_lightstyles_enable;
static cvar_t *gl_pt_dlights_enable;
static cvar_t *gl_pt_brushmodel_shadows_enable;
static cvar_t *gl_pt_aliasmodel_shadows_enable;
static cvar_t *gl_pt_bounce_factor;
static cvar_t *gl_pt_diffuse_map_enable;
static cvar_t *gl_pt_static_entity_lights_enable;

	
static GLhandleARB pt_program_handle;

static GLuint pt_node_texture = 0;
static GLuint pt_child_texture = 0;
static GLuint pt_bsp_lightref_texture = 0;

static GLuint pt_node0_buffer = 0;
static GLuint pt_node0_texture = 0;
static GLuint pt_node1_buffer = 0;
static GLuint pt_node1_texture = 0;
static GLuint pt_triangle_buffer = 0;
static GLuint pt_triangle_texture = 0;
static GLuint pt_vertex_buffer = 0;
static GLuint pt_vertex_texture = 0;
static GLuint pt_trilights_buffer = 0;
static GLuint pt_trilights_texture = 0;
static GLuint pt_lightref_buffer = 0;
static GLuint pt_lightref_texture = 0;
static GLuint pt_rand_texture = 0;

static GLint pt_frame_counter_loc = -1;
static GLint pt_entity_to_world_loc = -1;
static GLint pt_ao_radius_loc = -1;
static GLint pt_ao_color_loc = -1;
static GLint pt_bounce_factor_loc = -1;
		
		
static GLhandleARB vertex_shader;
static GLhandleARB fragment_shader;

static unsigned long int pt_bsp_texture_width = 0, pt_bsp_texture_height = 0;

static const GLcharARB* vertex_shader_source =
	"#version 120\n"
	"uniform mat4 entity_to_world = mat4(1);\n"
	"varying vec4 texcoords[5], color;\n"
	"void main()\n"
	"{\n"
	"	gl_Position = ftransform();\n"
	"	texcoords[0] = gl_MultiTexCoord0;\n"
	"	texcoords[1] = entity_to_world * gl_Vertex;\n"
	"	texcoords[2].xyz = vec3(0.0, 0.0, 0.0);\n"
	"	texcoords[3].xyz = normalize(mat3(entity_to_world) * gl_MultiTexCoord2.xyz);\n"
	"	texcoords[4] = gl_MultiTexCoord3;\n"
	"	color = gl_Color;\n"
	"}\n"
	"\n";

static const GLcharARB* fragment_shader_main_source = ""
#include "generated/pathtracing_shader_source.h"
	;

	
typedef float vec4_t[4];
static vec4_t pt_lerped[MAX_VERTS];

typedef struct entitylight_s
{
	vec3_t origin;
	vec3_t color;
	float intensity;
	int style;
	float radius;
	short clusters[PT_MAX_ENTITY_LIGHT_CLUSTERS];
	short light_index;
} entitylight_t;

typedef struct trilight_s
{
	short triangle_index;
	qboolean quad;
	msurface_t* surface;
	entitylight_t* entity;
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
static short pt_trilight_references[PT_MAX_TRI_LIGHT_REFS];
static int pt_cluster_light_references[PT_MAX_CLUSTERS * 2];
static entitylight_t pt_entitylights[PT_MAX_ENTITY_LIGHTS];
static short pt_lightstyle_sublists[MAX_LIGHTSTYLES];
static short pt_lightstyle_sublist_lengths[MAX_LIGHTSTYLES];
static vec3_t pt_cached_lightstyles[MAX_LIGHTSTYLES];

static short pt_num_nodes = 0;
static short pt_num_triangles = 0;
static short pt_num_vertices = 0;
static short pt_written_nodes = 0;
static short pt_previous_node = -1;
static short pt_num_lights = 0;
static int pt_num_trilight_references = 0;
static short pt_num_entitylights = 0;
static short pt_num_clusters = 0;
static short pt_num_used_nonstatic_lightstyles = 0;

static short pt_dynamic_vertices_offset = 0;
static short pt_dynamic_triangles_offset = 0;
static short pt_dynamic_entitylights_offset = 0;
static short pt_dynamic_lights_offset = 0;

static int pt_triangle_data[PT_MAX_TRIANGLES * 2];
static int pt_node0_data[PT_MAX_TRI_NODES * 4];
static int pt_node1_data[PT_MAX_TRI_NODES * 4];
static float pt_vertex_data[PT_MAX_VERTICES * 4];
static float pt_trilight_data[PT_MAX_TRI_LIGHTS * 4];

static int pt_last_update_ms = -1;

static GLenum pt_vertex_buffer_format = GL_NONE;
static int pt_vertex_stride = 0;


qboolean
R_PathtracingIsSupportedByGL(void)
{
	return R_VersionOfGLIsGreaterThanOrEqualTo(3, 3);
}

static void
PrintMissingGLFeaturesMessageAndDisablePathtracing()
{
	VID_Printf(PRINT_ALL, "ERROR: The OpenGL features necessary for pathtracing are not available.\n");
	
	if (gl_pt_enable)
		Cvar_ForceSet(gl_pt_enable->name, "0");
}

static void
ClearLightStyleCache(void)
{
	int i, j;
	for (i = 0; i < MAX_LIGHTSTYLES; ++i)
		for (j = 0; j < 3; ++j)
			pt_cached_lightstyles[i][j] = -1;
}

static void
ClearPathtracerState(void)
{
	ClearLightStyleCache();
	
	pt_num_nodes = 0;
	pt_num_triangles = 0;
	pt_num_vertices = 0;
	pt_written_nodes = 0;
	pt_previous_node = -1;
	pt_num_lights = 0;
	pt_num_trilight_references = 0;
	pt_num_entitylights = 0;
	pt_num_clusters = 0;
	pt_num_used_nonstatic_lightstyles = 0;

	pt_dynamic_vertices_offset = 0;
	pt_dynamic_triangles_offset = 0;
	pt_dynamic_entitylights_offset = 0;
	pt_dynamic_lights_offset = 0;
	
	pt_program_handle = 0;

	pt_node_texture = 0;
	pt_child_texture = 0;
	pt_bsp_lightref_texture = 0;

	pt_node0_buffer = 0;
	pt_node0_texture = 0;
	pt_node1_buffer = 0;
	pt_node1_texture = 0;
	pt_triangle_buffer = 0;
	pt_triangle_texture = 0;
	pt_vertex_buffer = 0;
	pt_vertex_texture = 0;
	pt_trilights_buffer = 0;
	pt_trilights_texture = 0;
	pt_lightref_buffer = 0;
	pt_lightref_texture = 0;
	pt_rand_texture = 0;

	pt_frame_counter_loc = -1;
	pt_entity_to_world_loc = -1;
	pt_ao_radius_loc = -1;
	pt_ao_color_loc = -1;
	pt_bounce_factor_loc = -1;
	
	pt_last_update_ms = -1;
	
	pt_vertex_buffer_format = GL_NONE;
	pt_vertex_stride = 0;
}

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

static qboolean
TriLightIsAnEntityLight(const trilight_t *light)
{
	return light->entity != NULL;
}

static qboolean
TriLightIsASkyPortal(const trilight_t *light)
{
	return !TriLightIsAnEntityLight(light) && (light->surface->texinfo->flags & SURF_SKY);
}

static int
LightSkyPortalAndStyleComparator(void const *a, void const *b)
{
	trilight_t *la = (trilight_t*)a;
	trilight_t *lb = (trilight_t*)b;
	
	int fa = (la->surface != NULL) ? (la->surface->texinfo->flags & SURF_SKY) : 0;
	int fb = (lb->surface != NULL) ? (lb->surface->texinfo->flags & SURF_SKY) : 0;
	
	if (fa == fb)
	{
		int sa = (la->entity != NULL) ? la->entity->style : 0;
		int sb = (lb->entity != NULL) ? lb->entity->style : 0;
		
		if (sa == sb)
		{
			return 0;
		}
		else if (sa > sb)
		{
			return +1;
		}
	}
	else if (fa > fb)
	{
		return +1;
	}
	
	return -1;
}

static void
AddPointLight(entitylight_t *entity)
{
	int poly_offset, j, k, light_index;
	trilight_t *light;
	
	/* Construct a tetrahedron. */
	
	poly_offset = pt_num_vertices;
	
	const vec3_t d = { 1.0, sqrt(2.0 / 3.0) * 0.5, sqrt(3.0) / 2.0 };
	
	const vec3_t tetrahedron_vertices[PT_NUM_ENTITY_VERTICES] = {
			{-d[0],-d[1],-d[2]},
			{ d[0],-d[1],-d[2]},
			{ 0.0,-d[1], d[2]},
			{ 0.0,+d[1],-1.0/12.0 * sqrt(3.0)},
		};
		
	static const short tetrahedron_indices[PT_NUM_ENTITY_TRILIGHTS][3] = {
			{0,1,2},
			{0,3,1},
			{1,3,2},
			{2,3,0},
		};
	
	for (j = 0; j < PT_NUM_ENTITY_VERTICES; ++j)
		for (k = 0; k < 3; ++k)
			pt_vertex_data[(poly_offset + j) * pt_vertex_stride + k] = tetrahedron_vertices[j][k] * entity->radius / 2.0 + entity->origin[k];
	
	pt_num_vertices += PT_NUM_ENTITY_VERTICES;
	
	entity->light_index = pt_num_lights;
	
	for (j = 0; j < PT_NUM_ENTITY_TRILIGHTS; ++j)
	{
		light_index = pt_num_lights++;
		light = pt_trilights + light_index;
		
		light->quad = false;
		light->triangle_index = pt_num_triangles++;
		light->surface = NULL;
		light->entity = entity;
		
		/* Store the triangle data. */
		
		pt_triangle_data[light->triangle_index * 2 + 0] = (poly_offset + tetrahedron_indices[j][0]) | ((poly_offset + tetrahedron_indices[j][1]) << 16);
		pt_triangle_data[light->triangle_index * 2 + 1] = poly_offset + tetrahedron_indices[j][2];
	}
}

/* Converts the trilight structure data within the given range into a form directly usable as a sampled buffer. */
static void
PackTriLightData(short start, short end)
{
	short m, j;
	mtexinfo_t *texinfo;
	trilight_t *light;

	for (m = start; m < end; ++m)
	{
		light = pt_trilights + m;
		
		if (TriLightIsAnEntityLight(light))
		{
			/* The light emitted by an entity is defined entirely by the entity itself. */
			
			for (j = 0; j < 3; ++j)
				pt_trilight_data[m * 4 + j] = light->entity->color[j] * light->entity->intensity * 32;
		}
		else
		{
			/* Light emitted by a non-entity light is defined by surface texture metadata. */
			
			texinfo = light->surface->texinfo;

			/* Calculate the radiant flux of the light. This stored vector is negated if the light is a quad. */
			
			for (j = 0; j < 3; ++j)
				pt_trilight_data[m * 4 + j] = (light->quad ? -1 : +1) * texinfo->image->reflectivity[j] * texinfo->radiance;
		}
		
		pt_trilight_data[m * 4 + 3] = IntBitsToFloat(light->triangle_index);
	}
}

static void
AddStaticLights(void)
{
	msurface_t *surf;
	mtexinfo_t *texinfo;
	trilight_t *light;
	int i, j, k, m;
	glpoly_t *p;
	float *v, x;
	int poly_offset;
	int light_index;
	mleaf_t *leaf, *other_leaf;
	byte *vis;
	int cluster;
	entitylight_t *entity;
	qboolean light_is_in_pvs;
	int style, previous_style;
	int num_direct_lights;
	qboolean sky_is_visible;

	/* Ensure that there is always an empty reference list at the first index. */
	
	if (pt_num_trilight_references < PT_MAX_TRI_LIGHT_REFS)
		pt_trilight_references[pt_num_trilight_references++] = -1;
		
	/* Reset the cluster light reference lists. */
	
	for (i = 0; i < sizeof(pt_cluster_light_references) / sizeof(pt_cluster_light_references[0]); ++i)
		pt_cluster_light_references[i] = 0;

	/* Visit each surface in the worldmodel. */
	
	for (i = 0; i < r_worldmodel->numsurfaces; ++i)
	{
		surf = r_worldmodel->surfaces + i;
		texinfo = surf->texinfo;
		if (!(texinfo->flags & SURF_WARP) && texinfo->radiance > 0)
		{			
			p = surf->polys;
		
			if (pt_num_vertices > (PT_MAX_VERTICES - p->numverts))
				continue;
		
			v = p->verts[0];
			
			poly_offset = pt_num_vertices;
		
			for (k = 0; k < p->numverts; k++, v += VERTEXSIZE)
			{
				/* Store this vertex of the polygon. */
				
				for (j = 0; j < 3; ++j)
					pt_vertex_data[pt_num_vertices * pt_vertex_stride + j] = v[j];

				pt_num_vertices++;	
			}
			
			/* Test to see if this polygon is a parallelogram. In which case a small trick is used to reduce the total number of lights. */
			
			int parallelogram_reflection = 0;
			
			if (p->numverts == 4)
			{
				/* Reflect each vertex and test whether the result matches the opposite vertex in the polygon. */
				
				int ind[7] = { poly_offset + 3, poly_offset, poly_offset + 1, poly_offset + 2, poly_offset + 3, poly_offset, poly_offset + 1 };

				for (k = 0; k < 4; ++k)
				{
					for (j = 0; j < 3; ++j)
					{
						x = pt_vertex_data[ind[k + 2] * pt_vertex_stride + j] + pt_vertex_data[ind[k] * pt_vertex_stride + j] - pt_vertex_data[ind[k + 1] * pt_vertex_stride + j];
						if (abs(x - pt_vertex_data[ind[k + 3] * pt_vertex_stride + j]) > 0.25)
							break;
					}
					if (j == 3)
					{
						parallelogram_reflection = k + 1;
						break;
					}
				}
			}
			
			if (parallelogram_reflection > 0)
			{
				/* Add a new triangle light and mark it as a quad (really a parallelogram). */
				
				if (pt_num_triangles >= PT_MAX_TRIANGLES || pt_num_lights >= PT_MAX_TRI_LIGHTS)
					continue;
				
				int ind[6] = { poly_offset + 3, poly_offset, poly_offset + 1, poly_offset + 2, poly_offset + 3, poly_offset };

				light_index = pt_num_lights++;
				light = pt_trilights + light_index;
				
				light->quad = true;
				light->triangle_index = pt_num_triangles++;
				light->surface = surf;
				light->entity = NULL;
													
				/* Store the triangle data. */
				
				pt_triangle_data[light->triangle_index * 2 + 0] = ind[parallelogram_reflection] | (ind[parallelogram_reflection + 1] << 16);
				pt_triangle_data[light->triangle_index * 2 + 1] = ind[parallelogram_reflection - 1];
			}
			else
			{		
				if (pt_num_triangles > (PT_MAX_TRIANGLES - p->numverts - 2) || pt_num_lights > (PT_MAX_TRI_LIGHTS - p->numverts - 2))
					continue;
		
				for (k = 2; k < p->numverts; k++)
				{
					/* Add a new triangle light for this segment of the polygon. */
										
					int ind[3] = { poly_offset, poly_offset + k - 1, poly_offset + k };

					light_index = pt_num_lights++;
					light = pt_trilights + light_index;
					
					light->quad = false;
					light->triangle_index = pt_num_triangles++;
					light->surface = surf;
					light->entity = NULL;
														
					/* Store the triangle data. */
					
					pt_triangle_data[light->triangle_index * 2 + 0] = ind[0] | (ind[1] << 16);
					pt_triangle_data[light->triangle_index * 2 + 1] = ind[2];
				}
			}
		}
	}

	/* Vist each static entity light. */
	
	for (i = 0; i < pt_num_entitylights; ++i)
	{
		entity = pt_entitylights + i;
		
		if ((entity->color[0] <= 0 && entity->color[1] <= 0 && entity->color[2] <= 0) || entity->intensity <= 0 || entity->radius <= 0)
			continue;
		
		if (pt_num_vertices > (PT_MAX_VERTICES - PT_NUM_ENTITY_VERTICES) || pt_num_triangles > (PT_MAX_TRIANGLES - PT_NUM_ENTITY_TRILIGHTS) || pt_num_lights > (PT_MAX_TRI_LIGHTS - PT_NUM_ENTITY_TRILIGHTS))
			continue;
		
		AddPointLight(entity);
	}
	
	/* Sort the lights such that sky portals come last in the light list, and the remaining lights are sorted by style. */
	
	qsort(pt_trilights, pt_num_lights, sizeof(pt_trilights[0]), LightSkyPortalAndStyleComparator);
	
	/* Get the number of lights which are not skyportals. */
	
	for (i = 0; i < pt_num_lights; ++i)
		if (TriLightIsASkyPortal(pt_trilights + i))
			break;
	
	num_direct_lights = i;
	
	/* Build the lightstyle sublists. Only direct lights are considered here beause skyportals can't have lightstyles.
		Note that lightstyle zero is always static (brightness and colour do not change during runtime). */
	
	previous_style = 0;
	style = 0;
	k = 0;
	
	for (i = 0; i < MAX_LIGHTSTYLES; ++i)
	{
		pt_lightstyle_sublists[i] = 0;
		pt_lightstyle_sublist_lengths[i] = 0;
	}
	
	for (m = 0; m < num_direct_lights; ++m)
	{
		light = pt_trilights + m;
		style = light->entity ? light->entity->style : 0;
		
		if (style != previous_style)
		{
			pt_lightstyle_sublists[previous_style] = k;
			pt_lightstyle_sublist_lengths[previous_style] = m - k;
			k = m;
			previous_style = style;
		}		
	}
	
	pt_lightstyle_sublists[style] = k;
	pt_lightstyle_sublist_lengths[style] = m - k;
	
	pt_num_used_nonstatic_lightstyles = 0;

	for (i = 1; i < MAX_LIGHTSTYLES; ++i)
	{
		if (pt_lightstyle_sublist_lengths[i] > 0)
			++pt_num_used_nonstatic_lightstyles;
	}

	/* Pack the light data into the buffer. */
	
	PackTriLightData(0, pt_num_lights);
							
	/* Visit each leaf in the worldmodel and build the list of visible lights for each cluster. */
		
	for (i = 0; i < r_worldmodel->numleafs; ++i)
	{
		if (pt_num_trilight_references >= PT_MAX_TRI_LIGHT_REFS)
			continue;
	
		leaf = r_worldmodel->leafs + i;
		
		if (leaf->contents == CONTENTS_SOLID || leaf->cluster == -1 || leaf->cluster >= PT_MAX_CLUSTERS)
			continue;
		
		/* Skip clusters which have already been processed. */

		if (pt_cluster_light_references[leaf->cluster * 2 + 0] != 0 || pt_cluster_light_references[leaf->cluster * 2 + 1] != 0)
			continue;
		
		/* Track the highest cluster index to determine how many clusters are in the worldmodel. */
		
		if ((leaf->cluster + 1) > pt_num_clusters)
			pt_num_clusters = leaf->cluster + 1;
		
		/* Get the PVS bits for this cluster. */
		
		vis = Mod_ClusterPVS(leaf->cluster, r_worldmodel);
		
		/* First a bitset is created indicating which lights are potentially visible, then a reference list is constructed from this. */
		
		static byte light_list_bits[(PT_MAX_TRI_LIGHTS + 7) / 8];
		memset(light_list_bits, 0, sizeof(light_list_bits));
		
		/* Go through every surface in this leaf and mark any light which corresponds to one of
			those surfaces. This is only done for skyportals. */
								
		for (m = num_direct_lights; m < pt_num_lights; ++m)
		{
			light = pt_trilights + m;
			
			for (k = 0; k < leaf->nummarksurfaces; ++k)
			{
				surf = leaf->firstmarksurface[k];
				
				if (light->surface == surf)
				{
					/* Mark this light for inclusion in the reference list. */
					light_list_bits[m >> 3] |= 1 << (m & 7);
					break;
				}
			}
		}
		
		sky_is_visible = false;
						
		/* Go through every visible leaf and build a list of the lights which have corresponding
			surfaces in that leaf. Skyportals are detected, but are not inserted into the visibility lists. */
		
		for (j = 0; j < r_worldmodel->numleafs; ++j)
		{
			other_leaf = r_worldmodel->leafs + j;
			
			if (other_leaf->contents == CONTENTS_SOLID)
				continue;
		
			cluster = other_leaf->cluster;

			if (cluster == -1 || cluster >= PT_MAX_CLUSTERS)
				continue;

			if (vis[cluster >> 3] & (1 << (cluster & 7)))
			{
				/* This leaf is visible, so look for any lightsources within it. */
				
				for (m = 0; m < pt_num_lights; ++m)
				{					
					if (light_list_bits[m >> 3] & (1 << (m & 7)))
					{
						/* This light was already marked visible, so it does not need to be processed again. */
						continue;
					}
					
					light_is_in_pvs = false;
										
					if (TriLightIsAnEntityLight(pt_trilights + m))
					{
						/* If this cluster is in the list of clusters that the light intersects then the light is visible. */
						for (k = 0; k < PT_MAX_ENTITY_LIGHT_CLUSTERS; ++k)
						{
							if (pt_trilights[m].entity->clusters[k] == cluster)
							{
								light_is_in_pvs = true;
								break;
							}
							else if (pt_trilights[m].entity->clusters[k] == -1)
							{
								break;
							}
						}
					}
					else
					{
						/* If this leaf contains the surface which this light was created from then the light is visible. */
						for (k = 0; k < other_leaf->nummarksurfaces; ++k)
						{
							surf = other_leaf->firstmarksurface[k];
						
							if (pt_trilights[m].surface == surf)
							{
								light_is_in_pvs = true;
								break;
							}
						}
					}
					
					if (light_is_in_pvs)
					{
						if (m < num_direct_lights)
						{
							/* This light is not a skyportal, so mark it for inclusion in the reference list. */
							light_list_bits[m >> 3] |= 1 << (m & 7);
						}
						else
						{
							/* Skyportals aren't added to the list of visible lights, but if any are visible then we need to raise the relevant flag. */
							sky_is_visible = true;
						}
					}
				}
			}
		}
		
		/* Construct the reference lists. */
			
		pt_cluster_light_references[leaf->cluster * 2 + 0] = pt_num_trilight_references * (sky_is_visible ? -1 : +1);

		for (m = 0; m < num_direct_lights; ++m)
		{
			if (light_list_bits[m >> 3] & (1 << (m & 7)))
			{	
				if (pt_num_trilight_references < PT_MAX_TRI_LIGHT_REFS)
				{
					pt_trilight_references[pt_num_trilight_references++] = m;
				}
			}
		}

		/* Insert multiple end-of-list markers in order to allocate space for dlights without needing
			 to re-arrange the lists. */
		
		for (m = 0; m < PT_MAX_CLUSTER_DLIGHTS + 1; ++m)
		{		
			if (pt_num_trilight_references < PT_MAX_TRI_LIGHT_REFS)
				pt_trilight_references[pt_num_trilight_references++] = -1;
		}
		
		pt_cluster_light_references[leaf->cluster * 2 + 1] = pt_num_trilight_references;

		for (m = num_direct_lights; m < pt_num_lights; ++m)
		{
			if (light_list_bits[m >> 3] & (1 << (m & 7)))
			{	
				if (pt_num_trilight_references < PT_MAX_TRI_LIGHT_REFS)
				{
					pt_trilight_references[pt_num_trilight_references++] = m;
				}
			}
		}

		if (pt_num_trilight_references < PT_MAX_TRI_LIGHT_REFS)
			pt_trilight_references[pt_num_trilight_references++] = -1;
	}
	
	/* Ensure that the reference at the end of the buffer is always an end-of-list marker. */
	pt_trilight_references[PT_MAX_TRI_LIGHT_REFS - 1] = -1;
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
	trinode_t *na = *(trinode_t**)a;
	trinode_t *nb = *(trinode_t**)b;
	float sa = na->surface_area;
	float sb = nb->surface_area;
	return (int)(sb - sa);
}

static int
TriNodeMortonCodeComparator(void const *a, void const *b)
{
	trinode_t *na = *(trinode_t**)a;
	trinode_t *nb = *(trinode_t**)b;
	return (int)na->morton_code - (int)nb->morton_code;
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

/* Applies a non-uniform scaling to the given 4x4 matrix in-place. */
static void 
MatrixScale(float m[16], float sx, float sy, float sz)
{
	int i,j;
	float s[3] = { sx, sy, sz };
	for (i = 0; i < 3; ++i)
		for (j = 0; j < 3; ++j)
		{
			m[i * 4 + j] *= s[i];
		}
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
	
	if (entity->flags & RF_DEPTHHACK)
		MatrixScale(m, .25f, .25f, .25f);
}

void
R_SetGLStateForPathtracing(const float entity_to_world_matrix[16])
{
	if (!qglUseProgramObjectARB || !qglUniformMatrix4fvARB || !R_PathtracingIsSupportedByGL())
	{
		PrintMissingGLFeaturesMessageAndDisablePathtracing();
		return;
	}
	
	qglUseProgramObjectARB(pt_program_handle);
	qglUniformMatrix4fvARB(pt_entity_to_world_loc, 1, GL_FALSE, entity_to_world_matrix);
}

void
R_ClearGLStateForPathtracing(void)
{
	if (!qglUseProgramObjectARB || !R_PathtracingIsSupportedByGL())
	{
		PrintMissingGLFeaturesMessageAndDisablePathtracing();
		return;
	}
	
	qglUseProgramObjectARB(0);
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
	
	for (i = 0; i < PT_MAX_NODE_DEPTH && num_added_nodes > 2; ++i)
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

	entitynode_n0 = NULL;
	entitynode_n1 = NULL;
	
	if (num_added_nodes > 2)
	{
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
	}
	
	WriteTriNodes(first_node_index, num_added_nodes);
	
	if (entitynode_n0)
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
	
	if (pt_num_vertices > (PT_MAX_VERTICES - alias_header->num_xyz))
		return;

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
			
			pt_vertex_data[pt_num_vertices * pt_vertex_stride + j] = lerp[j];
			
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

		if (pt_num_vertices > (PT_MAX_VERTICES - p->numverts))
			continue;
		
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
				
				pt_vertex_data[pt_num_vertices * pt_vertex_stride + j] = vertex[j];
				
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
						x = pt_vertex_data[ind[j] * pt_vertex_stride + m];

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
ParseEntityVector(vec3_t vec, const char* str)
{
	sscanf (str, "%f %f %f", &vec[0], &vec[1], &vec[2]);
}

static void
ClearEntityLightClusterList(entitylight_t *entity)
{
	int i;
	for (i = 0; i < PT_MAX_ENTITY_LIGHT_CLUSTERS; ++i)
		entity->clusters[i] = -1;
}

static void
ClearEntityLight(entitylight_t *entity)
{		
	int i;
	
	for (i = 0; i < 3; ++i)
		entity->origin[i] = entity->color[i] = 0;
	
	entity->style = 0;
	entity->intensity = 0;
	entity->radius = 0;
	entity->light_index = 0;
	
	ClearEntityLightClusterList(entity);
}

static qboolean
SphereIntersectsAnySolidLeaf(vec3_t origin, float radius)
{
	mnode_t *node_stack[PT_MAX_BSP_TREE_DEPTH];
	int stack_size;
	mnode_t *node;
	float d;
	cplane_t *plane;
	model_t *model;
	
	stack_size = 0;
	model = r_worldmodel;

	node_stack[stack_size++] = model->nodes;
	
	while (stack_size > 0)
	{
		node = node_stack[--stack_size];

		if (node->contents != -1)
		{
			if (node->contents == CONTENTS_SOLID || ((mleaf_t*)node)->cluster == -1)
				return true;
			else
				continue;
		}
		
		plane = node->plane;
		d = DotProduct(origin, plane->normal) - plane->dist;
		
		if (d > -radius)
		{
			if (stack_size < PT_MAX_BSP_TREE_DEPTH)
				node_stack[stack_size++] = node->children[0];
		}
		
		if (d < +radius)
		{
			if (stack_size < PT_MAX_BSP_TREE_DEPTH)
				node_stack[stack_size++] = node->children[1];
		}
	}
	
	return false;
}

static void
EnsureEntityLightDoesNotIntersectWalls(entitylight_t *entity)
{
	mleaf_t *leaf;
	
	leaf = Mod_PointInLeaf(entity->origin, r_worldmodel);
	
	if (!leaf || leaf->contents == CONTENTS_SOLID || leaf->cluster == -1)
	{
		VID_Printf(PRINT_DEVELOPER, "EnsureEntityLightDoesNotIntersectWalls: Entity's origin is within a wall.\n");
		entity->radius = 0;
		return;
	}
	
	/* If the entity intersects a solid wall then reduce it's radius by half repeatedly until either
		it becomes free or it becomes too small. */
	while (SphereIntersectsAnySolidLeaf(entity->origin, entity->radius) && entity->radius > 1.0f / 8.0f)
		entity->radius /= 2.0f;
}

static void
BuildClusterListForEntityLight(entitylight_t *entity)
{
	mnode_t *node_stack[PT_MAX_BSP_TREE_DEPTH];
	int i, stack_size;
	mnode_t *node;
	float d, r;
	cplane_t *plane;
	model_t *model;
	mleaf_t *leaf;
	short num_clusters;
	qboolean already_listed;
	
	stack_size = 0;
	r = entity->radius;
	model = r_worldmodel;
	num_clusters = 0;
	
	ClearEntityLightClusterList(entity);

	node_stack[stack_size++] = model->nodes;
	
	while (stack_size > 0)
	{
		node = node_stack[--stack_size];
		
		if (node->contents != -1)
		{
			leaf = (mleaf_t*)node;
			
			if (leaf->cluster == -1 || leaf->cluster >= PT_MAX_CLUSTERS)
				continue;
			
			already_listed = false;
			
			for (i = 0; i < num_clusters; ++i)
				if (entity->clusters[i] == leaf->cluster)
				{
					already_listed = true;
					break;
				}
				
			if (!already_listed && num_clusters < PT_MAX_ENTITY_LIGHT_CLUSTERS)
				entity->clusters[num_clusters++] = leaf->cluster;

			continue;
		}

		plane = node->plane;
		d = DotProduct(entity->origin, plane->normal) - plane->dist;
		
		if (d > -r)
		{
			if (stack_size < PT_MAX_BSP_TREE_DEPTH)
				node_stack[stack_size++] = node->children[0];
		}
		
		if (d < +r)
		{
			if (stack_size < PT_MAX_BSP_TREE_DEPTH)
				node_stack[stack_size++] = node->children[1];
		}
	}
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
		
		if (!(entity->flags & RF_WEAPONMODEL) && (entity->flags & (RF_FULLBRIGHT | RF_DEPTHHACK | RF_TRANSLUCENT | RF_BEAM | RF_NOSHADOW | RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM)))
			continue;
		
		model = entity->model;
		
		if (!model)
			continue;
		
		switch (model->type)
		{
			case mod_alias:
				if (gl_pt_aliasmodel_shadows_enable->value)
					AddAliasModel(entity, model);
				break;
			case mod_brush:
				if (gl_pt_brushmodel_shadows_enable->value)
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
UploadTextureBufferData(GLuint buffer, void *data, GLintptr offset, GLsizeiptr size)
{
	if (size == 0)
		return;
	
	if (qglBindBufferARB)
		qglBindBufferARB(GL_TEXTURE_BUFFER, buffer);
	else if (qglBindBuffer)
		qglBindBuffer(GL_TEXTURE_BUFFER, buffer);
	
	if (qglBufferSubDataARB)	
		qglBufferSubDataARB(GL_TEXTURE_BUFFER, offset, size, data);
	else
		qglBufferSubData(GL_TEXTURE_BUFFER, offset, size, data);
	
	if (qglBindBufferARB)
		qglBindBufferARB(GL_TEXTURE_BUFFER, 0);
	else if (qglBindBuffer)
		qglBindBuffer(GL_TEXTURE_BUFFER, 0);
}

static void *
MapTextureBufferRange(GLuint buffer, GLint offset, GLsizei length)
{
	if (qglBindBufferARB)
		qglBindBufferARB(GL_TEXTURE_BUFFER, buffer);
	else if (qglBindBuffer)
		qglBindBuffer(GL_TEXTURE_BUFFER, buffer);
	
	if (qglMapBufferRange)
		return qglMapBufferRange(GL_TEXTURE_BUFFER, offset, length, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
	
	return NULL;
}

static void *
MapTextureBuffer(GLuint buffer, GLenum access)
{
	if (qglBindBufferARB)
		qglBindBufferARB(GL_TEXTURE_BUFFER, buffer);
	else if (qglBindBuffer)
		qglBindBuffer(GL_TEXTURE_BUFFER, buffer);
	
	if (qglMapBufferARB)
		return qglMapBufferARB(GL_TEXTURE_BUFFER, access);
	else if (qglMapBuffer)
		return qglMapBuffer(GL_TEXTURE_BUFFER, access);
	
	return NULL;
}

static void
UnmapTextureBuffer()
{
	if (qglUnmapBufferARB)
		qglUnmapBufferARB(GL_TEXTURE_BUFFER);
	else if (qglUnmapBuffer)
		qglUnmapBuffer(GL_TEXTURE_BUFFER);
	
	if (qglBindBufferARB)
		qglBindBufferARB(GL_TEXTURE_BUFFER, 0);
	else if (qglBindBuffer)
		qglBindBuffer(GL_TEXTURE_BUFFER, 0);
}

static void
AddDLights(void)
{
	int i, j;
	const dlight_t* dl;
	entitylight_t* entity;
	
	for (i = 0; i < r_newrefdef.num_dlights; ++i)
	{
		dl = r_newrefdef.dlights + i;
		
		if ((dl->color[0] <= 0 && dl->color[1] <= 0 && dl->color[2] <= 0) || dl->intensity <= 0)
			continue;
		
		if (pt_num_entitylights >= PT_MAX_ENTITY_LIGHTS || pt_num_vertices > (PT_MAX_VERTICES - PT_NUM_ENTITY_VERTICES) ||
				pt_num_triangles > (PT_MAX_TRIANGLES - PT_NUM_ENTITY_TRILIGHTS) || pt_num_lights > (PT_MAX_TRI_LIGHTS - PT_NUM_ENTITY_TRILIGHTS))
			continue;

		entity = pt_entitylights + pt_num_entitylights++;
		
		ClearEntityLight(entity);
		
		for (j = 0; j < 3; ++j)
		{
			entity->origin[j] = dl->origin[j];
			entity->color[j] = dl->color[j];
			entity->intensity = dl->intensity;
		}

		entity->style = 0;
		entity->radius = 8;
		
		if ((entity->color[0] <= 0 && entity->color[1] <= 0 && entity->color[2] <= 0) || entity->intensity <= 0 || entity->radius <= 0)
			continue;
		
		AddPointLight(entity);
		BuildClusterListForEntityLight(entity);
	}
}

void
R_UpdatePathtracerForCurrentFrame(void)
{
	int i, j, k, m;
	lightstyle_t *lightstyle;
	float *mapped_buffer;
	entitylight_t *entity;
	int cluster, other_cluster;
	short *mapped_references;
	byte *vis;
	float* cached;
	int start_ms = 0, end_ms = 0, refresh_ms = 0, ms = 0;
	
	if (!R_PathtracingIsSupportedByGL())
	{
		PrintMissingGLFeaturesMessageAndDisablePathtracing();
		return;
	}	
	
	if (gl_pt_stats->value)
	{
		ms = Sys_Milliseconds();
		start_ms = ms;
		
		if (pt_last_update_ms != -1)		
			refresh_ms = ms - pt_last_update_ms;
		
		pt_last_update_ms = ms;
	}
	
	/* Clear the dynamic (moving) lightsource data by re-visiting the data ranges which were updated in the previous frame.
		There is no need to update the lightsource data itself as it's only necessary to remove the references. */
	
	if (pt_num_entitylights > pt_dynamic_entitylights_offset)
	{
		mapped_references = (short*)MapTextureBuffer(pt_lightref_buffer, GL_WRITE_ONLY);
		
		if (mapped_references)
		{
			/* Visit every cluster and clear it's dynamic light references. */
			for (cluster = 0; cluster < pt_num_clusters; ++cluster)
			{
				/* Locate the end of the static list, where the dynamic light references were appended. */
				
				k = abs(pt_cluster_light_references[cluster * 2 + 0]);

				while (pt_trilight_references[k] != -1)
					++k;

				/* Replace all of the dynamic light references with end-of-list markers. */
				for (i = 0; i < PT_MAX_CLUSTER_DLIGHTS; ++i)
					mapped_references[k + i] = -1;
			}
			
			UnmapTextureBuffer();
		}
	}
	
	/* Reset the dynamic data offsets. */
	
	pt_num_nodes = 0;
	pt_num_triangles = pt_dynamic_triangles_offset;
	pt_num_vertices = pt_dynamic_vertices_offset;
	pt_num_entitylights = pt_dynamic_entitylights_offset;
	pt_num_lights = pt_dynamic_lights_offset;
	pt_written_nodes = 0;
	pt_previous_node = -1;

	/* Add dynamic shadow-casting entity geometry. */	
	AddEntities();
	
	if (gl_pt_dlights_enable->value)
	{
		/* Add dynamic light emitters. */
		AddDLights();
	}
	
	/* Nullify the entry just past the last node, so that we can guarantee that traversal terminates correctly. */
	
	for (i = 0; i < 4; ++i)
	{
		pt_node0_data[pt_written_nodes * 4 + i] = 0;
		pt_node1_data[pt_written_nodes * 4 + i] = 0;
	}

	pt_written_nodes++;
		
	UploadTextureBufferData(pt_node0_buffer, pt_node0_data, 0, pt_written_nodes * 4 * sizeof(pt_node0_data[0]));
	UploadTextureBufferData(pt_node1_buffer, pt_node1_data, 0, pt_written_nodes * 4 * sizeof(pt_node1_data[0]));
	
	if (pt_num_triangles > pt_dynamic_triangles_offset)
	{
		UploadTextureBufferData(pt_triangle_buffer, pt_triangle_data + pt_dynamic_triangles_offset * 2, pt_dynamic_triangles_offset * 2 * sizeof(pt_triangle_data[0]), (pt_num_triangles - pt_dynamic_triangles_offset) * 2 * sizeof(pt_triangle_data[0]));
	}
	
	if (pt_num_vertices > pt_dynamic_vertices_offset)
	{
		UploadTextureBufferData(pt_vertex_buffer, pt_vertex_data + pt_dynamic_vertices_offset * pt_vertex_stride, pt_dynamic_vertices_offset * pt_vertex_stride * sizeof(pt_vertex_data[0]), (pt_num_vertices - pt_dynamic_vertices_offset) * pt_vertex_stride * sizeof(pt_vertex_data[0]));
	}

	if (gl_pt_lightstyles_enable->value && pt_num_used_nonstatic_lightstyles > 0)
	{
		/* Update the lightsource states with the current lightstyle states. */
		
		if (gl_config.map_buffer_range)
		{
			/* Map a subrange of the TBO for updating. */
			for (i = 1; i < MAX_LIGHTSTYLES; ++i)
			{
				j = pt_lightstyle_sublists[i];
				k = pt_lightstyle_sublist_lengths[i];
				
				if (k > 0)
				{
					lightstyle = r_newrefdef.lightstyles + i;

					cached = pt_cached_lightstyles[i];
				
					/* If the lightstyle hasn't changed since the last update, then skip it. */
					if (cached[0] == lightstyle->rgb[0] && cached[1] == lightstyle->rgb[1] && cached[2] == lightstyle->rgb[2])
						continue;
					
					for (m = 0; m < 3; ++m)
						cached[m] = lightstyle->rgb[m];
					
					mapped_buffer = (float*)MapTextureBufferRange(pt_trilights_buffer, j * sizeof(pt_trilight_data[0]) * 4, k * sizeof(pt_trilight_data[0]) * 4);
					
					if (!mapped_buffer)
						continue;
					
					j = 0;
					
					for (; j < k; ++j)
					{
						for (m = 0; m < 3; ++m)
							mapped_buffer[j * 4 + m] = pt_trilight_data[j * 4 + m] * cached[m];
						mapped_buffer[j * 4 + 3] = pt_trilight_data[j * 4 + 3];
					}
					
					UnmapTextureBuffer();
					mapped_buffer = NULL;
				}
			}
		}
		else
		{
			/* Mapping subranges is not possible, so map the entire buffer if any lightstyle update is necessary. */
			
			mapped_buffer = NULL;
			
			for (i = 1; i < MAX_LIGHTSTYLES; ++i)
			{
				j = pt_lightstyle_sublists[i];
				k = pt_lightstyle_sublist_lengths[i];
								
				if (k > 0)
				{
					lightstyle = r_newrefdef.lightstyles + i;

					cached = pt_cached_lightstyles[i];
				
					/* If the lightstyle hasn't changed since the last update, then skip it. */
					if (cached[0] == lightstyle->rgb[0] && cached[1] == lightstyle->rgb[1] && cached[2] == lightstyle->rgb[2])
						continue;
				
					for (m = 0; m < 3; ++m)
						cached[m] = lightstyle->rgb[m];
					
					k += j;

					/* The buffer is mapped here so that no mapping occurs if none of the lightstyles differ from their cached copies. */
					if (!mapped_buffer)
					{
						mapped_buffer = (float*)MapTextureBuffer(pt_trilights_buffer, GL_WRITE_ONLY);
						if (!mapped_buffer)
							break;
					}
					
					for (; j < k; ++j)
					{
						for (m = 0; m < 3; ++m)
							mapped_buffer[j * 4 + m] = pt_trilight_data[j * 4 + m] * cached[m];
						
						mapped_buffer[j * 4 + 3] = pt_trilight_data[j * 4 + 3];
					}
				}
			}
			
			if (mapped_buffer)
			{
				UnmapTextureBuffer();
				mapped_buffer = NULL;
			}
		}
	}

	if (gl_pt_dlights_enable->value)
	{
		/* Update the dynamic (moving) lightsources if necessary. */
		
		if (pt_num_entitylights > pt_dynamic_entitylights_offset)
		{
			/* Pack the dynamic light data into the buffer. */
			
			PackTriLightData(pt_dynamic_lights_offset, pt_num_lights);
		
			UploadTextureBufferData(pt_trilights_buffer, pt_trilight_data + pt_dynamic_lights_offset * 4, pt_dynamic_lights_offset * 4 * sizeof(pt_trilight_data[0]), (pt_num_lights - pt_dynamic_lights_offset) * 4 * sizeof(pt_trilight_data[0]));

			mapped_references = (short*)MapTextureBuffer(pt_lightref_buffer, GL_READ_WRITE);
			
			if (mapped_references)
			{
				/* Visit each dynamic entity light and add it into the light reference list of each cluster that it is
					potentially visible from. */
				
				for (i = pt_dynamic_entitylights_offset; i < pt_num_entitylights; ++i)
				{
					entity = pt_entitylights + i;
					
					for (j = 0; j < PT_MAX_ENTITY_LIGHT_CLUSTERS; ++j)
					{
						cluster = entity->clusters[j];
						
						if (cluster == -1)
							break;
						
						/* Get the PVS bits for this cluster. */
						
						vis = Mod_ClusterPVS(cluster, r_worldmodel);
						
						/* Visit every cluster which is visible from this cluster. The individual leaves don't	matter because
							they were already assigned an index to the first reference in their respective clusters. */
						
						for (other_cluster = 0; other_cluster < pt_num_clusters; ++other_cluster)
						{
							/* If this cluster is visible then update it's reference list. */
							
							if (vis[other_cluster >> 3] & (1 << (other_cluster & 7)))
							{
								/* Locate the end of the list, where dynamic light references can be appended. */
								
								k = abs(pt_cluster_light_references[other_cluster * 2 + 0]);

								while (mapped_references[k] != -1)
									++k;

								for (m = 0; m < PT_NUM_ENTITY_TRILIGHTS; ++m)
								{					
									/* Ensure that there is at least one end-of-list marker. */
									
									if (k >= PT_MAX_TRI_LIGHT_REFS || pt_trilight_references[k + 1] != -1 || mapped_references[k] != -1)
										continue;

									/* Insert the reference. */
									
									mapped_references[k++] = entity->light_index + m;
								}
							}
						}
					}
				}
				
				UnmapTextureBuffer();
			}
		}
	}

	/* Update the configuration uniform variables. */
	qglUseProgramObjectARB(pt_program_handle);
	qglUniform1fARB(pt_ao_radius_loc, gl_pt_ao_radius->value);
	qglUniform3fARB(pt_ao_color_loc, gl_pt_ao_color->value, gl_pt_ao_color->value, gl_pt_ao_color->value);
	qglUniform1fARB(pt_bounce_factor_loc, gl_pt_bounce_factor->value);
	qglUniform1iARB(pt_frame_counter_loc, r_framecount);	
	qglUseProgramObjectARB(0);
		
	/* Print the stats if necessary. */
	if (gl_pt_stats->value)
	{
		end_ms = Sys_Milliseconds();

		VID_Printf(PRINT_ALL, "pt_stats: f=%7d, n=%7d, t=%7d, v=%7d, w=%7d, l=%7d, c=%7d, r=%7d\n", r_framecount, pt_num_nodes,
						pt_num_triangles - pt_dynamic_triangles_offset, pt_num_vertices - pt_dynamic_vertices_offset, pt_written_nodes, pt_num_lights - pt_dynamic_lights_offset,
						end_ms - start_ms, refresh_ms);
	}
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
	tex_light_data = (int*)Z_Malloc(num_texels * 4 * sizeof(int));
	
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
					
					tex_light_data[i * 4 + 0 + j] = pt_cluster_light_references[cluster * 2 + 0];
					tex_light_data[i * 4 + 2 + j] = pt_cluster_light_references[cluster * 2 + 1];
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
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32I, pt_bsp_texture_width, pt_bsp_texture_height, 0, GL_RGBA_INTEGER, GL_INT, tex_light_data);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	Z_Free(tex_node_data);
	Z_Free(tex_child_data);
	Z_Free(tex_light_data);
}


/* Parses a single entity and extracts the fields which are interesting for the purposes of
	pathtracing. This function is mostly based on ED_ParseEdict from g_spawn.c, and mimics some
	of the logic of CreateDirectLights from qrad3's lightmap.c */
static char *
ParseEntityDictionary(char *data)
{
	char keyname[256];
	const char *com_token;
	char classname[256];
	char origin[256];
	char color[256];
	char light[256];
	char style[256];
	entitylight_t *entity;

	classname[0] = 0;
	origin[0] = 0;
	color[0] = 0;
	light[0] = 0;
	style[0] = 0;
	
	/* go through all the dictionary pairs */
	while (1)
	{
		/* parse key */
		com_token = COM_Parse(&data);

		if (com_token[0] == '}')
		{
			break;
		}

		if (!data)
		{
			VID_Printf(ERR_DROP, "ParseEntityDictionary: EOF without closing brace\n");
		}

		Q_strlcpy(keyname, com_token, sizeof(keyname));

		/* parse value */
		com_token = COM_Parse(&data);

		if (!data)
		{
			VID_Printf(ERR_DROP, "ParseEntityDictionary: EOF without closing brace\n");
		}

		if (com_token[0] == '}')
		{
			VID_Printf(ERR_DROP, "ParseEntityDictionary: closing brace without data\n");
		}
		
		if (!Q_stricmp(keyname, "classname"))
		{
			Q_strlcpy(classname, com_token, sizeof(classname));
		}
		else if (!Q_stricmp(keyname, "origin"))
		{
			Q_strlcpy(origin, com_token, sizeof(origin));
		}
		else if (!Q_stricmp(keyname, "_color"))
		{
			Q_strlcpy(color, com_token, sizeof(color));
		}
		else if (!Q_stricmp(keyname, "_light") || !Q_stricmp(keyname, "light"))
		{
			Q_strlcpy(light, com_token, sizeof(light));
		}
		else if (!Q_stricmp(keyname, "_style") || !Q_stricmp(keyname, "style"))
		{
			Q_strlcpy(style, com_token, sizeof(style));
		}
	}

	if (!Q_stricmp(classname, "light") && pt_num_entitylights < PT_MAX_ENTITY_LIGHTS)
	{
		entity = pt_entitylights + pt_num_entitylights++;

		ClearEntityLight(entity);

		ParseEntityVector(entity->origin, origin);
		
		if (color[0])
			ParseEntityVector(entity->color, color);
		else
			entity->color[0] = entity->color[1] = entity->color[2] = 1.0;
		
		entity->intensity = atof(light);
		entity->style = atof(style);
		
		/* The default radius is set to stay within the QUAKED bounding box specified for lights in g_misc.c */
		entity->radius = 8;

		/* Enforce the same defaults and restrictions that qrad3 enforces. */

		if (entity->intensity == 0)
			entity->intensity = 300;
		
		if (entity->style < 0 || entity->style >= MAX_LIGHTSTYLES)
			entity->style = 0;
		
		EnsureEntityLightDoesNotIntersectWalls(entity);
		BuildClusterListForEntityLight(entity);
	}

	return data;
}


/* Parses the entities from the given string (which should have been taken directly from the
	entities lump of a map). This function is mostly based on SpawnEntities from g_spawn.c */
static void
ParseStaticEntityLights(char *entitystring)
{
	const char *com_token;

	if (!entitystring)
	{
		return;
	}

	/* parse ents */
	while (1)
	{
		/* parse the opening brace */
		com_token = COM_Parse(&entitystring);

		if (!entitystring)
		{
			break;
		}

		if (com_token[0] != '{')
		{
			VID_Printf(ERR_DROP, "ParseStaticEntityLights: found %s when expecting {\n", com_token);
			return;
		}

		if (pt_num_entitylights >= PT_MAX_ENTITY_LIGHTS)
			break;
				
		entitystring = ParseEntityDictionary(entitystring);
	}

}

void
R_PreparePathtracer(void)
{
	FreeModelData();
	ClearLightStyleCache();
	
	if (!R_PathtracingIsSupportedByGL())
	{
		PrintMissingGLFeaturesMessageAndDisablePathtracing();
		return;
	}
	
	pt_last_update_ms = -1;

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
	CreateTextureBuffer(&pt_vertex_buffer, &pt_vertex_texture, pt_vertex_buffer_format, PT_MAX_VERTICES * pt_vertex_stride * sizeof(GLfloat));
	CreateTextureBuffer(&pt_trilights_buffer, &pt_trilights_texture, GL_RGBA32F, PT_MAX_TRI_LIGHTS * 4 * sizeof(GLfloat));
	CreateTextureBuffer(&pt_lightref_buffer, &pt_lightref_texture, GL_R16I, PT_MAX_TRI_LIGHT_REFS * sizeof(GLshort));
	
	pt_num_nodes = 0;
	pt_num_triangles = 0;
	pt_num_vertices = 0;
	pt_written_nodes = 0;
	pt_previous_node = -1;
	pt_num_lights = 0;
	pt_num_trilight_references = 0;
	pt_num_entitylights = 0;
	pt_num_clusters = 0;

	if (gl_pt_static_entity_lights_enable->value)
		ParseStaticEntityLights(Mod_EntityString());

	VID_Printf(PRINT_DEVELOPER, "R_PreparePathtracer: %d static entity light-emitters\n", pt_num_entitylights);

	AddStaticLights();
	
	VID_Printf(PRINT_DEVELOPER, "R_PreparePathtracer: %d static trilights\n", pt_num_lights);

	AddStaticBSP();

	VID_Printf(PRINT_DEVELOPER, "R_PreparePathtracer: Static BSP texture size is %dx%d\n", pt_bsp_texture_width, pt_bsp_texture_height);
	
	UploadTextureBufferData(pt_trilights_buffer, pt_trilight_data, 0, pt_num_lights * 4 * sizeof(pt_trilight_data[0]));
	UploadTextureBufferData(pt_lightref_buffer, pt_trilight_references, 0, pt_num_trilight_references * sizeof(pt_trilight_references[0]));
	
	UploadTextureBufferData(pt_triangle_buffer, pt_triangle_data, 0, pt_num_triangles * 2 * sizeof(pt_triangle_data[0]));
	UploadTextureBufferData(pt_vertex_buffer, pt_vertex_data, 0, pt_num_vertices * pt_vertex_stride * sizeof(pt_vertex_data[0]));

	pt_dynamic_vertices_offset = pt_num_vertices;
	pt_dynamic_triangles_offset = pt_num_triangles;
	pt_dynamic_entitylights_offset = pt_num_entitylights;
	pt_dynamic_lights_offset = pt_num_lights;
		
	qglActiveTextureARB(GL_TEXTURE2_ARB);
	glBindTexture(GL_TEXTURE_2D, pt_node_texture);
	qglActiveTextureARB(GL_TEXTURE3_ARB);
	glBindTexture(GL_TEXTURE_2D, pt_child_texture);
	qglActiveTextureARB(GL_TEXTURE4_ARB);
	glBindTexture(GL_TEXTURE_BUFFER, pt_node0_texture);
	qglActiveTextureARB(GL_TEXTURE5_ARB);
	glBindTexture(GL_TEXTURE_BUFFER, pt_node1_texture);
	qglActiveTextureARB(GL_TEXTURE6_ARB);
	glBindTexture(GL_TEXTURE_BUFFER, pt_vertex_texture);
	qglActiveTextureARB(GL_TEXTURE7_ARB);
	glBindTexture(GL_TEXTURE_BUFFER, pt_triangle_texture);
	qglActiveTextureARB(GL_TEXTURE8_ARB);
	glBindTexture(GL_TEXTURE_BUFFER, pt_trilights_texture);
	qglActiveTextureARB(GL_TEXTURE9_ARB);
	glBindTexture(GL_TEXTURE_BUFFER, pt_lightref_texture);
	qglActiveTextureARB(GL_TEXTURE10_ARB);
	glBindTexture(GL_TEXTURE_2D, pt_bsp_lightref_texture);
	qglActiveTextureARB(GL_TEXTURE11_ARB);
	glBindTexture(GL_TEXTURE_2D_ARRAY, pt_rand_texture);
	qglActiveTextureARB(GL_TEXTURE0_ARB);
}
	
static void
FreeRandom(void)
{
	DeleteTexture(&pt_rand_texture);
}

static void
InitRandom(void)
{
	int i;
	static const int texture_size = PT_RAND_TEXTURE_SIZE, num_layers = 8;
	GLubyte texels[texture_size * texture_size * 2 * num_layers];
	
	for (i = 0; i < sizeof(texels) / sizeof(texels[0]); ++i)
		texels[i] = randk();
	
	glGenTextures(1, &pt_rand_texture);
	glBindTexture(GL_TEXTURE_2D_ARRAY, pt_rand_texture);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qglTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RG8, texture_size, texture_size, num_layers, 0, GL_RG, GL_UNSIGNED_BYTE, texels);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
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

static void
FreeShaderPrograms(void)
{
	pt_frame_counter_loc = -1;
	pt_entity_to_world_loc = -1;
	pt_ao_radius_loc = -1;
	pt_ao_color_loc = -1;
	pt_bounce_factor_loc = -1;

	if (vertex_shader)
	{
		qglDeleteObjectARB(vertex_shader);
		vertex_shader = 0;
	}
	
	if (fragment_shader)
	{
		qglDeleteObjectARB(fragment_shader);
		fragment_shader = 0;
	}
	
	if (pt_program_handle)
	{
		qglDeleteObjectARB(pt_program_handle);
		pt_program_handle = 0;
	}
}

static void
ConstructFragmentShaderSource(GLhandleARB shader)
{
	static const GLcharARB* version = "#version 330\n";
	
	static GLcharARB config[1024];

	snprintf(config, sizeof(config),
			"#define NUM_BOUNCES %d\n"
			"#define NUM_SHADOW_SAMPLES %d\n"
			"#define NUM_LIGHT_SAMPLES %d\n"
			"#define NUM_SKY_SAMPLES %d\n"
			"#define NUM_AO_SAMPLES %d\n"
			"#define TRI_SHADOWS_ENABLE %d\n"
			"#define DIFFUSE_MAP_ENABLE %d\n"
			"#define RAND_TEX_SIZE %d\n",
			MAX(0, (int)gl_pt_bounces->value),
			MAX(0, (int)gl_pt_shadow_samples->value),
			MAX(0, (int)gl_pt_light_samples->value),
			gl_pt_sky_enable->value ? MAX(0, (int)gl_pt_sky_samples->value) : 0,
			gl_pt_ao_enable->value ? MAX(0, (int)gl_pt_ao_samples->value) : 0,
			MAX(0, (int)gl_pt_aliasmodel_shadows_enable->value | (int)gl_pt_brushmodel_shadows_enable->value),
			MAX(0, (int)gl_pt_diffuse_map_enable->value),
			PT_RAND_TEXTURE_SIZE
		);
	
	const GLcharARB* strings[] = { version, config, fragment_shader_main_source };
	
	qglShaderSourceARB(shader, sizeof(strings) / sizeof(strings[0]), strings, NULL);
}

static void
CreateShaderPrograms(void)
{
	GLint status = 0;

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

	ConstructFragmentShaderSource(fragment_shader);
	
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
	qglUniform1iARB(qglGetUniformLocationARB(pt_program_handle, "randtex"), 11);
		
	pt_frame_counter_loc = qglGetUniformLocationARB(pt_program_handle, "frame");
	pt_entity_to_world_loc = qglGetUniformLocationARB(pt_program_handle, "entity_to_world");
	pt_ao_radius_loc = qglGetUniformLocationARB(pt_program_handle, "ao_radius");
	pt_ao_color_loc = qglGetUniformLocationARB(pt_program_handle, "ao_color");
	pt_bounce_factor_loc = qglGetUniformLocationARB(pt_program_handle, "bounce_factor");
		
	qglUseProgramObjectARB(0);
}
	
static void
RecompileShaderPrograms(void)
{
	FreeShaderPrograms();
	CreateShaderPrograms();
	VID_Printf(PRINT_ALL, "Shaders recompiled.\n");
}

static void
PrintStaticInfo(void)
{
	VID_Printf(PRINT_ALL, "%6d trilight references\n", pt_num_trilight_references);
	VID_Printf(PRINT_ALL, "%6d clusters\n", pt_num_clusters);
	VID_Printf(PRINT_ALL, "%6d used nonstatic lightstyles\n", pt_num_used_nonstatic_lightstyles);
	VID_Printf(PRINT_ALL, "%6d static vertices\n", pt_dynamic_vertices_offset);
	VID_Printf(PRINT_ALL, "%6d static triangles\n", pt_dynamic_triangles_offset);
	VID_Printf(PRINT_ALL, "%6d static entity lights\n", pt_dynamic_entitylights_offset);
	VID_Printf(PRINT_ALL, "%6d static trilights\n", pt_dynamic_lights_offset);
}
	
void
R_InitPathtracing(void)
{
	ClearPathtracerState();

#define GET_PT_CVAR(x, d) x = Cvar_Get( #x, d, CVAR_ARCHIVE);
	GET_PT_CVAR(gl_pt_enable, "0")
	GET_PT_CVAR(gl_pt_stats, "0")
	GET_PT_CVAR(gl_pt_bounces, "0")
	GET_PT_CVAR(gl_pt_shadow_samples, "1")
	GET_PT_CVAR(gl_pt_light_samples, "1")
	GET_PT_CVAR(gl_pt_sky_enable, "1")
	GET_PT_CVAR(gl_pt_sky_samples, "1")
	GET_PT_CVAR(gl_pt_ao_enable, "0")
	GET_PT_CVAR(gl_pt_ao_radius, "150")
	GET_PT_CVAR(gl_pt_ao_color, "1")
	GET_PT_CVAR(gl_pt_ao_samples, "1")
	GET_PT_CVAR(gl_pt_translucent_surfaces_enable, "1")
	GET_PT_CVAR(gl_pt_lightstyles_enable, "1")
	GET_PT_CVAR(gl_pt_dlights_enable, "1")
	GET_PT_CVAR(gl_pt_brushmodel_shadows_enable, "1")
	GET_PT_CVAR(gl_pt_aliasmodel_shadows_enable, "1")
	GET_PT_CVAR(gl_pt_bounce_factor, "0.75")
	GET_PT_CVAR(gl_pt_diffuse_map_enable, "1")
	GET_PT_CVAR(gl_pt_static_entity_lights_enable, "1")
#undef CVAR

	Cmd_AddCommand("gl_pt_recompile_shaders", RecompileShaderPrograms);
	Cmd_AddCommand("gl_pt_print_static_info", PrintStaticInfo);

	if (!R_PathtracingIsSupportedByGL())
	{
		PrintMissingGLFeaturesMessageAndDisablePathtracing();
		return;
	}
	
	pt_vertex_buffer_format = (gl_config.texture_buffer_objects_rgb || R_VersionOfGLIsGreaterThanOrEqualTo(4, 0)) ? GL_RGB32F : GL_RGBA32F;
	pt_vertex_stride = (pt_vertex_buffer_format == GL_RGB32F) ? 3 : 4;

	InitRandom();
	CreateShaderPrograms();
}

void
R_ShutdownPathtracing(void)
{
	if (qglActiveTextureARB)
	{
		qglActiveTextureARB(GL_TEXTURE2_ARB);
		glBindTexture(GL_TEXTURE_2D, 0);
		qglActiveTextureARB(GL_TEXTURE3_ARB);
		glBindTexture(GL_TEXTURE_2D, 0);
		qglActiveTextureARB(GL_TEXTURE4_ARB);
		glBindTexture(GL_TEXTURE_BUFFER, 0);
		qglActiveTextureARB(GL_TEXTURE5_ARB);
		glBindTexture(GL_TEXTURE_BUFFER, 0);
		qglActiveTextureARB(GL_TEXTURE6_ARB);
		glBindTexture(GL_TEXTURE_BUFFER, 0);
		qglActiveTextureARB(GL_TEXTURE7_ARB);
		glBindTexture(GL_TEXTURE_BUFFER, 0);
		qglActiveTextureARB(GL_TEXTURE8_ARB);
		glBindTexture(GL_TEXTURE_BUFFER, 0);
		qglActiveTextureARB(GL_TEXTURE9_ARB);
		glBindTexture(GL_TEXTURE_BUFFER, 0);
		qglActiveTextureARB(GL_TEXTURE10_ARB);
		glBindTexture(GL_TEXTURE_2D, 0);
		qglActiveTextureARB(GL_TEXTURE11_ARB);
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
		qglActiveTextureARB(GL_TEXTURE0_ARB);
	}
	
	Cmd_RemoveCommand("gl_pt_recompile_shaders");
	Cmd_RemoveCommand("gl_pt_print_static_info");

	FreeModelData();
	FreeShaderPrograms();
	FreeRandom();
	
	ClearPathtracerState();
}

