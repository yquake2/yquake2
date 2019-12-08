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
 * Local header for the OpenGL3 refresher.
 *
 * =======================================================================
 */


#ifndef SRC_CLIENT_REFRESH_GL3_HEADER_LOCAL_H_
#define SRC_CLIENT_REFRESH_GL3_HEADER_LOCAL_H_

#ifdef IN_IDE_PARSER
  // this is just a hack to get proper auto-completion in IDEs:
  // using system headers for their parsers/indexers but glad for real build
  // (in glad glFoo is just a #define to glad_glFoo or sth, which screws up autocompletion)
  // (you may have to configure your IDE to #define IN_IDE_PARSER, but not for building!)
  #define GL_GLEXT_PROTOTYPES 1
  #include <GL/gl.h>
  #include <GL/glext.h>
#else
  #include "../glad/include/glad/glad.h"
#endif

#include "../../ref_shared.h"

#include "HandmadeMath.h"

#if 0 // only use this for development ..
#define STUB_ONCE(msg) do { \
		static int show=1; \
		if(show) { \
			show = 0; \
			R_Printf(PRINT_ALL, "STUB: %s() %s\n", __FUNCTION__, msg); \
		} \
	} while(0);
#else // .. so make this a no-op in released code
#define STUB_ONCE(msg)
#endif

// a wrapper around glVertexAttribPointer() to stay sane
// (caller doesn't have to cast to GLintptr and then void*)
static inline void
qglVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLintptr offset)
{
	glVertexAttribPointer(index, size, type, normalized, stride, (const void*)offset);
}

static inline void
qglVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset)
{
	glVertexAttribIPointer(index, size, type, stride, (void*)offset);
}

// attribute locations for vertex shaders
enum {
	GL3_ATTRIB_POSITION   = 0,
	GL3_ATTRIB_TEXCOORD   = 1, // for normal texture
	GL3_ATTRIB_LMTEXCOORD = 2, // for lightmap
	GL3_ATTRIB_COLOR      = 3, // per-vertex color
	GL3_ATTRIB_NORMAL     = 4, // vertex normal
	GL3_ATTRIB_LIGHTFLAGS = 5  // uint, each set bit means "dyn light i affects this surface"
};

// TODO: do we need the following configurable?
static const int gl3_solid_format = GL_RGB;
static const int gl3_alpha_format = GL_RGBA;
static const int gl3_tex_solid_format = GL_RGB;
static const int gl3_tex_alpha_format = GL_RGBA;

extern unsigned gl3_rawpalette[256];
extern unsigned d_8to24table[256];

typedef struct
{
	const char *renderer_string;
	const char *vendor_string;
	const char *version_string;
	const char *glsl_version_string;

	int major_version;
	int minor_version;

	// ----

	qboolean anisotropic; // is GL_EXT_texture_filter_anisotropic supported?
	qboolean debug_output; // is GL_ARB_debug_output supported?
	qboolean stencil; // Do we have a stencil buffer?

	qboolean useBigVBO; // workaround for AMDs windows driver for fewer calls to glBufferData()

	// ----

	float max_anisotropy;
} gl3config_t;

typedef struct
{
	GLuint shaderProgram;
	GLint uniLmScales;
	hmm_vec4 lmScales[4];
} gl3ShaderInfo_t;

typedef struct
{
	GLfloat gamma;
	GLfloat intensity;
	GLfloat intensity2D; // for HUD, menus etc

		// entries of std140 UBOs are aligned to multiples of their own size
		// so we'll need to pad accordingly for following vec4
		GLfloat _padding;

	hmm_vec4 color;
} gl3UniCommon_t;

typedef struct
{
	hmm_mat4 transMat4;
} gl3Uni2D_t;

typedef struct
{
	hmm_mat4 transProjMat4;
	hmm_mat4 transViewMat4;
	hmm_mat4 transModelMat4;

	GLfloat scroll; // for SURF_FLOWING
	GLfloat time; // for warping surfaces like water & possibly other things
	GLfloat alpha; // for translucent surfaces (water, glass, ..)
	GLfloat overbrightbits; // gl3_overbrightbits, applied to lightmaps (and elsewhere to models)
	GLfloat particleFadeFactor; // gl3_particle_fade_factor, higher => less fading out towards edges

		GLfloat _padding[3]; // again, some padding to ensure this has right size
} gl3Uni3D_t;

extern const hmm_mat4 gl3_identityMat4;

typedef struct
{
	vec3_t origin;
	GLfloat _padding;
	vec3_t color;
	GLfloat intensity;
} gl3UniDynLight;

typedef struct
{
	gl3UniDynLight dynLights[MAX_DLIGHTS];
	GLuint numDynLights;
	GLfloat _padding[3];
} gl3UniLights_t;

enum {
	// width and height used to be 128, so now we should be able to get the same lightmap data
	// that used 32 lightmaps before into one, so 4 lightmaps should be enough
	BLOCK_WIDTH = 1024,
	BLOCK_HEIGHT = 512,
	LIGHTMAP_BYTES = 4,
	MAX_LIGHTMAPS = 4,
	MAX_LIGHTMAPS_PER_SURFACE = MAXLIGHTMAPS // 4
};

typedef struct
{
	// TODO: what of this do we need?
	qboolean fullscreen;

	int prev_mode;

	unsigned char *d_16to8table;

	// each lightmap consists of 4 sub-lightmaps allowing changing shadows on the same surface
	// used for switching on/off light and stuff like that.
	// most surfaces only have one really and the remaining for are filled with dummy data
	GLuint lightmap_textureIDs[MAX_LIGHTMAPS][MAX_LIGHTMAPS_PER_SURFACE]; // instead of lightmap_textures+i use lightmap_textureIDs[i]

	GLuint currenttexture; // bound to GL_TEXTURE0
	int currentlightmap; // lightmap_textureIDs[currentlightmap] bound to GL_TEXTURE1
	GLuint currenttmu; // GL_TEXTURE0 or GL_TEXTURE1

	//float camera_separation;
	//enum stereo_modes stereo_mode;

	GLuint currentVAO;
	GLuint currentVBO;
	GLuint currentEBO;
	GLuint currentShaderProgram;
	GLuint currentUBO;

	// NOTE: make sure si2D is always the first shaderInfo (or adapt GL3_ShutdownShaders())
	gl3ShaderInfo_t si2D;      // shader for rendering 2D with textures
	gl3ShaderInfo_t si2Dcolor; // shader for rendering 2D with flat colors
	gl3ShaderInfo_t si3Dlm;        // a regular opaque face (e.g. from brush) with lightmap
	// TODO: lm-only variants for gl_lightmap 1
	gl3ShaderInfo_t si3Dtrans;     // transparent is always w/o lightmap
	gl3ShaderInfo_t si3DcolorOnly; // used for beams - no lightmaps
	gl3ShaderInfo_t si3Dturb;      // for water etc - always without lightmap
	gl3ShaderInfo_t si3DlmFlow;    // for flowing/scrolling things with lightmap (conveyor, ..?)
	gl3ShaderInfo_t si3DtransFlow; // for transparent flowing/scrolling things (=> no lightmap)
	gl3ShaderInfo_t si3Dsky;       // guess what..
	gl3ShaderInfo_t si3Dsprite;    // for sprites
	gl3ShaderInfo_t si3DspriteAlpha; // for sprites with alpha-testing

	gl3ShaderInfo_t si3Dalias;      // for models
	gl3ShaderInfo_t si3DaliasColor; // for models w/ flat colors

	// NOTE: make sure siParticle is always the last shaderInfo (or adapt GL3_ShutdownShaders())
	gl3ShaderInfo_t siParticle; // for particles. surprising, right?

	GLuint vao3D, vbo3D; // for brushes etc, using 10 floats and one uint as vertex input (x,y,z, s,t, lms,lmt, normX,normY,normZ ; lightFlags)

	// the next two are for gl3config.useBigVBO == true
	int vbo3Dsize;
	int vbo3DcurOffset;

	GLuint vaoAlias, vboAlias, eboAlias; // for models, using 9 floats as (x,y,z, s,t, r,g,b,a)
	GLuint vaoParticle, vboParticle; // for particles, using 9 floats (x,y,z, size,distance, r,g,b,a)

	// UBOs and their data
	gl3UniCommon_t uniCommonData;
	gl3Uni2D_t uni2DData;
	gl3Uni3D_t uni3DData;
	gl3UniLights_t uniLightsData;
	GLuint uniCommonUBO;
	GLuint uni2DUBO;
	GLuint uni3DUBO;
	GLuint uniLightsUBO;

} gl3state_t;

extern gl3config_t gl3config;
extern gl3state_t gl3state;

extern viddef_t vid;

extern refdef_t gl3_newrefdef;

extern int gl3_visframecount; /* bumped when going to a new PVS */
extern int gl3_framecount; /* used for dlight push checking */

extern int gl3_viewcluster, gl3_viewcluster2, gl3_oldviewcluster, gl3_oldviewcluster2;

extern int c_brush_polys, c_alias_polys;

/* NOTE: struct image_s* is what re.RegisterSkin() etc return so no gl3image_s!
 *       (I think the client only passes the pointer around and doesn't know the
 *        definition of this struct, so this being different from struct image_s
 *        in ref_gl should be ok)
 */
typedef struct image_s
{
	char name[MAX_QPATH];               /* game path, including extension */
	imagetype_t type;
	int width, height;                  /* source image */
	//int upload_width, upload_height;    /* after power of two and picmip */
	int registration_sequence;          /* 0 = free */
	struct msurface_s *texturechain;    /* for sort-by-texture world drawing */
	GLuint texnum;                      /* gl texture binding */
	float sl, tl, sh, th;               /* 0,0 - 1,1 unless part of the scrap */
	// qboolean scrap; // currently unused
	qboolean has_alpha;

} gl3image_t;

enum {MAX_GL3TEXTURES = 1024};

// include this down here so it can use gl3image_t
#include "model.h"

typedef struct
{
	int internal_format;
	int current_lightmap_texture; // index into gl3state.lightmap_textureIDs[]

	//msurface_t *lightmap_surfaces[MAX_LIGHTMAPS]; - no more lightmap chains, lightmaps are rendered multitextured

	int allocated[BLOCK_WIDTH];

	/* the lightmap texture data needs to be kept in
	   main memory so texsubimage can update properly */
	byte lightmap_buffers[MAX_LIGHTMAPS_PER_SURFACE][4 * BLOCK_WIDTH * BLOCK_HEIGHT];
} gl3lightmapstate_t;

extern gl3model_t *gl3_worldmodel;
extern gl3model_t *currentmodel;
extern entity_t *currententity;

extern float gl3depthmin, gl3depthmax;

extern cplane_t frustum[4];

extern vec3_t gl3_origin;

extern gl3image_t *gl3_notexture; /* use for bad textures */
extern gl3image_t *gl3_particletexture; /* little dot for particles */

extern int gl_filter_min;
extern int gl_filter_max;

static inline void
GL3_UseProgram(GLuint shaderProgram)
{
	if(shaderProgram != gl3state.currentShaderProgram)
	{
		gl3state.currentShaderProgram = shaderProgram;
		glUseProgram(shaderProgram);
	}
}

static inline void
GL3_BindVAO(GLuint vao)
{
	if(vao != gl3state.currentVAO)
	{
		gl3state.currentVAO = vao;
		glBindVertexArray(vao);
	}
}

static inline void
GL3_BindVBO(GLuint vbo)
{
	if(vbo != gl3state.currentVBO)
	{
		gl3state.currentVBO = vbo;
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
	}
}

static inline void
GL3_BindEBO(GLuint ebo)
{
	if(ebo != gl3state.currentEBO)
	{
		gl3state.currentEBO = ebo;
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	}
}

extern void GL3_BufferAndDraw3D(const gl3_3D_vtx_t* verts, int numVerts, GLenum drawMode);

extern qboolean GL3_CullBox(vec3_t mins, vec3_t maxs);
extern void GL3_RotateForEntity(entity_t *e);

// gl3_sdl.c
extern int GL3_InitContext(void* win);
extern int GL3_PrepareForWindow(void);
extern qboolean GL3_IsVsyncActive(void);
extern void GL3_EndFrame(void);
extern void GL3_SetVsync(void);
extern void GL3_ShutdownContext(void);

// gl3_misc.c
extern void GL3_InitParticleTexture(void);
extern void GL3_ScreenShot(void);
extern void GL3_SetDefaultState(void);

// gl3_model.c
extern int registration_sequence;
extern void GL3_Mod_Init(void);
extern void GL3_Mod_FreeAll(void);
extern void GL3_BeginRegistration(char *model);
extern struct model_s * GL3_RegisterModel(char *name);
extern void GL3_EndRegistration(void);
extern void GL3_Mod_Modellist_f(void);
extern byte* GL3_Mod_ClusterPVS(int cluster, gl3model_t *model);
extern mleaf_t* GL3_Mod_PointInLeaf(vec3_t p, gl3model_t *model);

// gl3_draw.c
extern void GL3_Draw_InitLocal(void);
extern void GL3_Draw_ShutdownLocal(void);
extern gl3image_t * GL3_Draw_FindPic(char *name);
extern void GL3_Draw_GetPicSize(int *w, int *h, char *pic);
extern int GL3_Draw_GetPalette(void);

extern void GL3_Draw_PicScaled(int x, int y, char *pic, float factor);
extern void GL3_Draw_StretchPic(int x, int y, int w, int h, char *pic);
extern void GL3_Draw_CharScaled(int x, int y, int num, float scale);
extern void GL3_Draw_TileClear(int x, int y, int w, int h, char *pic);
extern void GL3_Draw_Fill(int x, int y, int w, int h, int c);
extern void GL3_Draw_FadeScreen(void);
extern void GL3_Draw_Flash(const float color[4], float x, float y, float w, float h);
extern void GL3_Draw_StretchRaw(int x, int y, int w, int h, int cols, int rows, byte *data);

// gl3_image.c

static inline void
GL3_SelectTMU(GLenum tmu)
{
	if(gl3state.currenttmu != tmu)
	{
		glActiveTexture(tmu);
		gl3state.currenttmu = tmu;
	}
}

extern void GL3_TextureMode(char *string);
extern void GL3_Bind(GLuint texnum);
extern void GL3_BindLightmap(int lightmapnum);
extern gl3image_t *GL3_LoadPic(char *name, byte *pic, int width, int realwidth,
                               int height, int realheight, imagetype_t type, int bits);
extern gl3image_t *GL3_FindImage(char *name, imagetype_t type);
extern gl3image_t *GL3_RegisterSkin(char *name);
extern void GL3_ShutdownImages(void);
extern void GL3_FreeUnusedImages(void);
extern void GL3_ImageList_f(void);

// gl3_light.c
extern void GL3_MarkLights(dlight_t *light, int bit, mnode_t *node);
extern void GL3_PushDlights(void);
extern void GL3_LightPoint(vec3_t p, vec3_t color);
extern void GL3_BuildLightMap(msurface_t *surf, int offsetInLMbuf, int stride);

// gl3_lightmap.c
#define GL_LIGHTMAP_FORMAT GL_RGBA

extern void GL3_LM_InitBlock(void);
extern void GL3_LM_UploadBlock(void);
extern qboolean GL3_LM_AllocBlock(int w, int h, int *x, int *y);
extern void GL3_LM_BuildPolygonFromSurface(msurface_t *fa);
extern void GL3_LM_CreateSurfaceLightmap(msurface_t *surf);
extern void GL3_LM_BeginBuildingLightmaps(gl3model_t *m);
extern void GL3_LM_EndBuildingLightmaps(void);

// gl3_warp.c
extern void GL3_EmitWaterPolys(msurface_t *fa);
extern void GL3_SubdivideSurface(msurface_t *fa, gl3model_t* loadmodel);

extern void GL3_SetSky(char *name, float rotate, vec3_t axis);
extern void GL3_DrawSkyBox(void);
extern void GL3_ClearSkyBox(void);
extern void GL3_AddSkySurface(msurface_t *fa);


// gl3_surf.c
extern void GL3_SurfInit(void);
extern void GL3_SurfShutdown(void);
extern void GL3_DrawGLPoly(msurface_t *fa);
extern void GL3_DrawGLFlowingPoly(msurface_t *fa);
extern void GL3_DrawTriangleOutlines(void);
extern void GL3_DrawAlphaSurfaces(void);
extern void GL3_DrawBrushModel(entity_t *e);
extern void GL3_DrawWorld(void);
extern void GL3_MarkLeaves(void);

// gl3_mesh.c
extern void GL3_DrawAliasModel(entity_t *e);
extern void GL3_ResetShadowAliasModels(void);
extern void GL3_DrawAliasShadows(void);
extern void GL3_ShutdownMeshes(void);

// gl3_shaders.c

extern qboolean GL3_RecreateShaders(void);
extern qboolean GL3_InitShaders(void);
extern void GL3_ShutdownShaders(void);
extern void GL3_UpdateUBOCommon(void);
extern void GL3_UpdateUBO2D(void);
extern void GL3_UpdateUBO3D(void);
extern void GL3_UpdateUBOLights(void);

// ############ Cvars ###########

extern cvar_t *gl_msaa_samples;
extern cvar_t *r_vsync;
extern cvar_t *gl_retexturing;
extern cvar_t *vid_fullscreen;
extern cvar_t *r_mode;
extern cvar_t *r_customwidth;
extern cvar_t *r_customheight;

extern cvar_t *gl_nolerp_list;
extern cvar_t *gl_nobind;
extern cvar_t *r_lockpvs;
extern cvar_t *r_novis;

extern cvar_t *gl_cull;
extern cvar_t *gl_zfix;
extern cvar_t *r_fullbright;

extern cvar_t *r_norefresh;
extern cvar_t *gl_lefthand;
extern cvar_t *r_gunfov;
extern cvar_t *r_farsee;
extern cvar_t *r_drawworld;

extern cvar_t *vid_gamma;
extern cvar_t *gl3_intensity;
extern cvar_t *gl3_intensity_2D;
extern cvar_t *gl_anisotropic;
extern cvar_t *gl_texturemode;

extern cvar_t *r_lightlevel;
extern cvar_t *gl3_overbrightbits;
extern cvar_t *gl3_particle_fade_factor;
extern cvar_t *gl3_particle_square;

extern cvar_t *r_modulate;
extern cvar_t *gl_lightmap;
extern cvar_t *gl_shadows;

extern cvar_t *gl3_debugcontext;

#endif /* SRC_CLIENT_REFRESH_GL3_HEADER_LOCAL_H_ */
