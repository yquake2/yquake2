#include "../header/local.h"

image_t *
LoadWal ( char *name )
{
	miptex_t    *mt;
	int width, height, ofs;
	image_t     *image;

	ri.FS_LoadFile( name, (void **) &mt );

	if ( !mt )
	{
		ri.Con_Printf( PRINT_ALL, "LoadWall: can't load %s\n", name );
		return ( r_notexture );
	}

	width = LittleLong( mt->width );
	height = LittleLong( mt->height );
	ofs = LittleLong( mt->offsets [ 0 ] );

	image = R_LoadPic( name, (byte *) mt + ofs, width, height, it_wall, 8 );

	ri.FS_FreeFile( (void *) mt );

	return ( image );
}     

