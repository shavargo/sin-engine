/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// draw.c

#include "gl_local.h"

image_t		*draw_chars;

extern	qboolean	scrap_dirty;
void Scrap_Upload (void);


/*
===============
Draw_InitLocal
===============
*/
void Draw_InitLocal (void)
{
	/* Q2 .pcx / SiN .tga */
	draw_chars = GL_FindImage ("pics/conchars.pcx", it_pic);
	if (!draw_chars)
		draw_chars = GL_FindImage ("pics/conchars.tga", it_pic);
	if (!draw_chars)
		draw_chars = r_notexture;
	GL_Bind( draw_chars->texnum );
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}



/*
================
Draw_Char

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_Char (int x, int y, int num)
{
	int				row, col;
	float			frow, fcol, size;

	num &= 255;
	
	if ( (num&127) == 32 )
		return;		// space

	if (y <= -8)
		return;			// totally off screen

	row = num>>4;
	col = num&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

	GL_Bind (draw_chars->texnum);

	y = vid.height - y;

	qglBegin (GL_QUADS);
	qglTexCoord2f (fcol, frow);
	qglVertex2f (x, y);
	qglTexCoord2f (fcol, frow + size);
	qglVertex2f (x, y-8);
	qglTexCoord2f (fcol + size, frow + size);
	qglVertex2f (x+8, y-8);
	qglTexCoord2f (fcol + size, frow);
	qglVertex2f (x+8, y);
	qglEnd ();
}

/*
=============
Draw_FindPic
=============
*/
image_t	*Draw_FindPic (char *name)
{
	image_t *gl;
	char	fullname[MAX_QPATH];

	if (name[0] != '/' && name[0] != '\\')
	{
		const char *ext = strrchr(name, '.');
		if (ext && (!Q_stricmp(ext, ".tga") || !Q_stricmp(ext, ".pcx")))
		{
			Com_sprintf (fullname, sizeof(fullname), "pics/%s", name);
			gl = GL_FindImage (fullname, it_pic);
		}
		else
		{
			/* SiN Steam pics are .tga; try that first to avoid .pcx miss spam */
			Com_sprintf (fullname, sizeof(fullname), "pics/%s.tga", name);
			gl = GL_FindImage (fullname, it_pic);
			if (!gl)
			{
				Com_sprintf (fullname, sizeof(fullname), "pics/%s.pcx", name);
				gl = GL_FindImage (fullname, it_pic);
			}
		}
#ifdef SIN_MP
		/* Layout often asks for "ch1"; assets are ch1_on.tga */
		if (!gl && name[0] == 'c' && name[1] == 'h' && name[2] >= '1' && name[2] <= '5' && !name[3])
		{
			Com_sprintf (fullname, sizeof(fullname), "pics/%s_on.tga", name);
			gl = GL_FindImage (fullname, it_pic);
		}
#endif
	}
	else
		gl = GL_FindImage (name+1, it_pic);

	return gl;
}

/*
=============
Draw_GetPicSize
=============
*/
void Draw_GetPicSize (int *w, int *h, char *pic)
{
	image_t *gl;

	gl = Draw_FindPic (pic);
	if (!gl)
	{
		*w = *h = -1;
		return;
	}
	*w = gl->width;
	*h = gl->height;
}

/*
=============
Draw_StretchPic
=============
*/
void Draw_StretchPic (int x, int y, int w, int h, char *pic)
{
	image_t *gl;

	gl = Draw_FindPic (pic);
	if (!gl)
	{
		static int	miss_stretch;
		if ((miss_stretch++ % 200) == 0)
			ri.Con_Printf (PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	if (scrap_dirty)
		Scrap_Upload ();

	if ( ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) ) && !gl->has_alpha)
		qglDisable (GL_ALPHA_TEST);

	GL_Bind (gl->texnum);
	y = vid.height - y;
	qglBegin (GL_QUADS);
	qglTexCoord2f (gl->sl, gl->tl);
	qglVertex2f (x, y);
	qglTexCoord2f (gl->sl, gl->th);
	qglVertex2f (x, y-h);
	qglTexCoord2f (gl->sh, gl->th);
	qglVertex2f (x+w, y-h);
	qglTexCoord2f (gl->sh, gl->tl);
	qglVertex2f (x+w, y);
	qglEnd ();

	if ( ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) ) && !gl->has_alpha)
		qglEnable (GL_ALPHA_TEST);
}


/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, char *pic)
{
	image_t *gl;

	gl = Draw_FindPic (pic);
	if (!gl)
	{
		static int	miss_pic;
		if ((miss_pic++ % 200) == 0)
			ri.Con_Printf (PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}
	if (scrap_dirty)
		Scrap_Upload ();

	if ( ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) ) && !gl->has_alpha)
		qglDisable (GL_ALPHA_TEST);

	GL_Bind (gl->texnum);
	y = vid.height - y;
	qglBegin (GL_QUADS);
	qglTexCoord2f (gl->sl, gl->tl);
	qglVertex2f (x, y);
	qglTexCoord2f (gl->sl, gl->th);
	qglVertex2f (x, y-gl->height);
	qglTexCoord2f (gl->sh, gl->th);
	qglVertex2f (x+gl->width, y-gl->height);
	qglTexCoord2f (gl->sh, gl->tl);
	qglVertex2f (x+gl->width, y);
	qglEnd ();

	if ( ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) )  && !gl->has_alpha)
		qglEnable (GL_ALPHA_TEST);
}

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h, char *pic)
{
	image_t	*image;

	image = Draw_FindPic (pic);
	if (!image)
	{
		ri.Con_Printf (PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	if ( ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) )  && !image->has_alpha)
		qglDisable (GL_ALPHA_TEST);

	GL_Bind (image->texnum);
	y = vid.height - y;
	qglBegin (GL_QUADS);
	qglTexCoord2f (x/64.0, y/64.0);
	qglVertex2f (x, y);
	qglTexCoord2f ( (x+w)/64.0, y/64.0);
	qglVertex2f (x+w, y);
	qglTexCoord2f ( (x+w)/64.0, (y-h)/64.0);
	qglVertex2f (x+w, y-h);
	qglTexCoord2f ( x/64.0, (y-h)/64.0 );
	qglVertex2f (x, y-h);
	qglEnd ();

	if ( ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) )  && !image->has_alpha)
		qglEnable (GL_ALPHA_TEST);
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, float r, float g, float b)
{
	qglDisable (GL_TEXTURE_2D);
	qglColor3f (r, g, b);
	y = vid.height - y;
	qglBegin (GL_QUADS);
	qglVertex2f (x, y);
	qglVertex2f (x+w, y);
	qglVertex2f (x+w, y-h);
	qglVertex2f (x, y-h);
	qglEnd ();
	qglColor3f (1,1,1);
	qglEnable (GL_TEXTURE_2D);
}

static void Draw_Fill_Old (int x, int y, int w, int h, int c)
{
	union
	{
		unsigned	c;
		byte		v[4];
	} color;

	if ( (unsigned)c > 255)
		ri.Sys_Error (ERR_FATAL, "Draw_Fill: bad color");

	qglDisable (GL_TEXTURE_2D);

	color.c = d_8to24table[c];
	qglColor3f (color.v[0]/255.0,
		color.v[1]/255.0,
		color.v[2]/255.0);

	y = vid.height - y;
	qglBegin (GL_QUADS);

	qglVertex2f (x, y);
	qglVertex2f (x+w, y);
	qglVertex2f (x+w, y-h);
	qglVertex2f (x, y-h);

	qglEnd ();
	qglColor3f (1,1,1);
	qglEnable (GL_TEXTURE_2D);
}

//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (float r, float g, float b, float a)
{
	qglEnable (GL_BLEND);
	qglDisable (GL_TEXTURE_2D);
	if (a < 1.0f) {
		qglDisable(GL_ALPHA_TEST);
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	qglColor4f (r, g, b, a);
	qglBegin (GL_QUADS);

	qglVertex2f (0,0);
	qglVertex2f (vid.width, 0);
	qglVertex2f (vid.width, vid.height);
	qglVertex2f (0, vid.height);

	qglEnd ();
	qglColor4f (1,1,1,1);
	qglEnable (GL_TEXTURE_2D);
	qglDisable (GL_BLEND);
	if (a < 1.0f) {
		qglEnable(GL_ALPHA_TEST);
	}
}


//====================================================================


/*
=============
Draw_StretchRaw
=============
*/
extern unsigned	r_rawpalette[256];

void Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data)
{
	unsigned	image32[256*256];
	unsigned char image8[256*256];
	int			i, j, trows;
	byte		*source;
	int			frac, fracstep;
	float		hscale;
	int			row;
	float		t;

	GL_Bind (0);

	if (rows<=256)
	{
		hscale = 1;
		trows = rows;
	}
	else
	{
		hscale = rows/256.0;
		trows = 256;
	}
	t = rows*hscale / 256;

	if ( !qglColorTableEXT )
	{
		unsigned *dest;

		for (i=0 ; i<trows ; i++)
		{
			row = (int)(i*hscale);
			if (row > rows)
				break;
			source = data + cols*row;
			dest = &image32[i*256];
			fracstep = cols*0x10000/256;
			frac = fracstep >> 1;
			for (j=0 ; j<256 ; j++)
			{
				dest[j] = r_rawpalette[source[frac>>16]];
				frac += fracstep;
			}
		}

		qglTexImage2D (GL_TEXTURE_2D, 0, gl_tex_solid_format, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, image32);
	}
	else
	{
		unsigned char *dest;

		for (i=0 ; i<trows ; i++)
		{
			row = (int)(i*hscale);
			if (row > rows)
				break;
			source = data + cols*row;
			dest = &image8[i*256];
			fracstep = cols*0x10000/256;
			frac = fracstep >> 1;
			for (j=0 ; j<256 ; j++)
			{
				dest[j] = source[frac>>16];
				frac += fracstep;
			}
		}

		qglTexImage2D( GL_TEXTURE_2D, 
			           0, 
					   GL_COLOR_INDEX8_EXT, 
					   256, 256, 
					   0, 
					   GL_COLOR_INDEX, 
					   GL_UNSIGNED_BYTE, 
					   image8 );
	}
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) ) 
		qglDisable (GL_ALPHA_TEST);

	y = vid.height - y;
	qglBegin (GL_QUADS);
	qglTexCoord2f (0, 0);
	qglVertex2f (x, y);
	qglTexCoord2f (1, 0);
	qglVertex2f (x+w, y);
	qglTexCoord2f (1, t);
	qglVertex2f (x+w, y-h);
	qglTexCoord2f (0, t);
	qglVertex2f (x, y-h);
	qglEnd ();

	if ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) ) 
		qglEnable (GL_ALPHA_TEST);
}


/*
=============================================================

SiN Console 3D Drawing Functions

=============================================================
*/

conInfo_t consoleInfo;

int SINMDL_GetDefHandle (struct model_s *mdl)
{
	(void)mdl;
	return -1;
}

static void MatrixTransformVector (vec3_t in, float mat[3][3], vec3_t out)
{
	out[0] = in[0] * mat[0][0] + in[1] * mat[1][0] + in[2] * mat[2][0];
	out[1] = in[0] * mat[0][1] + in[1] * mat[1][1] + in[2] * mat[2][1];
	out[2] = in[0] * mat[0][2] + in[1] * mat[1][2] + in[2] * mat[2][2];
}

static void DoDraw_Fill3D (vec3_t org, float width, float height, vec3_t u, vec3_t v, float r, float g, float b, float a)
{
	vec3_t origin, svup, svright, point;

	qglDisable (GL_TEXTURE_2D);

	if (a < 1.0f)
	{
		qglDisable (GL_ALPHA_TEST);
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qglEnable (GL_BLEND);
	}

	qglColor4f (r, g, b, a);

	origin[0] = org[0];
	origin[1] = org[1];
	origin[2] = org[2];

	VectorCopy (v, svup);
	VectorCopy (u, svright);
	qglBegin (GL_QUADS);

	// lower left
	VectorCopy (origin, point);
	qglVertex3fv (point);

	// upper left
	VectorMA (origin, height, svup, point);
	qglVertex3fv (point);

	// upper right
	VectorMA (origin, height, svup, point);
	VectorMA (point, width, svright, point);
	qglVertex3fv (point);

	// lower right
	VectorMA (origin, width, svright, point);
	qglVertex3fv (point);

	qglEnd ();
	qglColor3f (1, 1, 1);
	qglEnable (GL_TEXTURE_2D);

	if (a < 1.0f)
	{
		qglEnable (GL_ALPHA_TEST);
		qglDisable (GL_BLEND);
	}
}

void Draw_Fill3D (float x1, float y1, float width, float height, float r, float g, float b, float a)
{
	vec3_t origin;
	vec3_t org1;
	float mat[3][3];

	origin[0] = x1 * consoleInfo.surface_width;
	origin[1] = y1 * consoleInfo.surface_height;
	origin[2] = 0;

	mat[0][0] = consoleInfo.u[0]; mat[0][1] = consoleInfo.u[1]; mat[0][2] = consoleInfo.u[2];
	mat[1][0] = consoleInfo.v[0]; mat[1][1] = consoleInfo.v[1]; mat[1][2] = consoleInfo.v[2];
	mat[2][0] = consoleInfo.n[0]; mat[2][1] = consoleInfo.n[1]; mat[2][2] = consoleInfo.n[2];

	MatrixTransformVector (origin, mat, org1);

	DoDraw_Fill3D (org1,
		width * consoleInfo.surface_width,
		height * consoleInfo.surface_height,
		consoleInfo.u, consoleInfo.v,
		r, g, b, a * consoleInfo.alpha);
}

static void DoDraw_AltFill3D (vec3_t org, float width, float height, vec3_t u, vec3_t v, float r, float g, float b, float a)
{
	vec3_t origin, svup, svright, point;

	qglDisable (GL_TEXTURE_2D);

	if (a < 1.0f)
	{
		qglDisable (GL_ALPHA_TEST);
		qglBlendFunc (GL_DST_COLOR, GL_ZERO);
		qglEnable (GL_BLEND);
	}

	qglColor4f (r, g, b, a);

	origin[0] = org[0];
	origin[1] = org[1];
	origin[2] = org[2];

	VectorCopy (v, svup);
	VectorCopy (u, svright);
	qglBegin (GL_QUADS);

	// lower left
	VectorCopy (origin, point);
	qglVertex3fv (point);

	// upper left
	VectorMA (origin, height, svup, point);
	qglVertex3fv (point);

	// upper right
	VectorMA (origin, height, svup, point);
	VectorMA (point, width, svright, point);
	qglVertex3fv (point);

	// lower right
	VectorMA (origin, width, svright, point);
	qglVertex3fv (point);

	qglEnd ();
	qglColor3f (1, 1, 1);
	qglEnable (GL_TEXTURE_2D);

	if (a < 1.0f)
	{
		qglEnable (GL_ALPHA_TEST);
		qglDisable (GL_BLEND);
	}
}

void Draw_AltFill3D (float x1, float y1, float width, float height, float r, float g, float b, float a)
{
	vec3_t origin;
	vec3_t org1;
	float mat[3][3];

	origin[0] = x1 * consoleInfo.surface_width;
	origin[1] = y1 * consoleInfo.surface_height;
	origin[2] = 0;

	mat[0][0] = consoleInfo.u[0]; mat[0][1] = consoleInfo.u[1]; mat[0][2] = consoleInfo.u[2];
	mat[1][0] = consoleInfo.v[0]; mat[1][1] = consoleInfo.v[1]; mat[1][2] = consoleInfo.v[2];
	mat[2][0] = consoleInfo.n[0]; mat[2][1] = consoleInfo.n[1]; mat[2][2] = consoleInfo.n[2];

	MatrixTransformVector (origin, mat, org1);

	DoDraw_AltFill3D (org1,
		width * consoleInfo.surface_width,
		height * consoleInfo.surface_height,
		consoleInfo.u, consoleInfo.v,
		r, g, b, a * consoleInfo.alpha);
}

static void DoDraw_Char3D (vec3_t coord, vec3_t u, vec3_t v, int num, float font_width, float font_height, float r, float g, float b, float a)
{
	int row, col;
	float frow, fcol, size;
	vec3_t svup, svright;
	vec3_t origin, point;

	num &= 255;
	if ((num & 127) == 32)
		return;

	origin[0] = coord[0];
	origin[1] = coord[1];
	origin[2] = coord[2];

	row = num >> 4;
	col = num & 15;
	frow = (float)row * 0.0625f;
	fcol = (float)col * 0.0625f;
	size = 0.0625f;

	VectorCopy (v, svup);
	VectorCopy (u, svright);

	if (draw_chars)
		GL_Bind (draw_chars->texnum);

	qglColor4f (r, g, b, a);

	if (a < 1.0f) {
		qglDisable (GL_ALPHA_TEST);
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qglEnable (GL_BLEND);
	}

	qglBegin (GL_QUADS);

	// lower left
	qglTexCoord2f (fcol, frow + size);
	VectorCopy (origin, point);
	qglVertex3fv (point);

	// upper left
	qglTexCoord2f (fcol, frow);
	VectorMA (origin, font_height, svup, point);
	qglVertex3fv (point);

	// upper right
	qglTexCoord2f (fcol + size, frow);
	VectorMA (origin, font_height, svup, point);
	VectorMA (point, font_width, svright, point);
	qglVertex3fv (point);

	// lower right
	qglTexCoord2f (fcol + size, frow + size);
	VectorMA (origin, font_width, svright, point);
	qglVertex3fv (point);

	qglEnd ();

	if (a < 1.0f) {
		qglDisable (GL_BLEND);
		qglEnable (GL_ALPHA_TEST);
	}

	qglColor4f (1, 1, 1, 1);
}

void Draw_Char3D (float x, float y, int c, int rows, int cols, float r, float g, float b, float a)
{
	vec3_t origin, org1;
	float mat[3][3];
	float font_width = consoleInfo.surface_width / (float)cols;
	float font_height = consoleInfo.surface_height / (float)rows;

	origin[0] = x * consoleInfo.surface_width;
	origin[1] = y * consoleInfo.surface_height;
	origin[2] = 0;

	mat[0][0] = consoleInfo.u[0]; mat[0][1] = consoleInfo.u[1]; mat[0][2] = consoleInfo.u[2];
	mat[1][0] = consoleInfo.v[0]; mat[1][1] = consoleInfo.v[1]; mat[1][2] = consoleInfo.v[2];
	mat[2][0] = consoleInfo.n[0]; mat[2][1] = consoleInfo.n[1]; mat[2][2] = consoleInfo.n[2];

	MatrixTransformVector (origin, mat, org1);
	DoDraw_Char3D (org1, consoleInfo.u, consoleInfo.v, c, font_width, font_height, r, g, b, a * consoleInfo.alpha);
}

void Flush_Char3D (float r, float g, float b, float a)
{
	(void)r; (void)g; (void)b; (void)a;
}

static void DoDraw_Pic3D (vec3_t coord, vec3_t u, vec3_t v, const char *pic, float scalex, float scaley, float r, float g, float b, float a)
{
	vec3_t svup, svright;
	vec3_t origin, point;
	float width, height;
	image_t *gl;
	float xepsilon, yepsilon;

	origin[0] = coord[0];
	origin[1] = coord[1];
	origin[2] = coord[2];
	gl = Draw_FindPic ((char *)pic);
	if (!gl) {
		ri.Con_Printf (PRINT_DEVELOPER, "Can't find pic: %s\n", pic);
		return;
	}

	if (scrap_dirty) {
		Scrap_Upload ();
	}

	qglColor4f (r, g, b, a);
	GL_Bind (gl->texnum);

	if (a < 1.0f) {
		qglDisable (GL_ALPHA_TEST);
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qglEnable (GL_BLEND);
	}

	width = (float)gl->width;
	height = (float)gl->height;
	xepsilon = 0.5f / width;
	yepsilon = 0.5f / height;
	width = width * scalex;
	height = height * scaley;

	VectorCopy (v, svup);
	VectorCopy (u, svright);

	qglBegin (GL_QUADS);

	// lower left
	qglTexCoord2f (gl->sl + xepsilon, gl->th - yepsilon);
	VectorCopy (origin, point);
	qglVertex3fv (point);

	// upper left
	qglTexCoord2f (gl->sl + xepsilon, gl->tl + yepsilon);
	VectorMA (origin, height, svup, point);
	qglVertex3fv (point);

	// upper right
	qglTexCoord2f (gl->sh - xepsilon, gl->tl + yepsilon);
	VectorMA (origin, height, svup, point);
	VectorMA (point, width, svright, point);
	qglVertex3fv (point);

	// lower right
	qglTexCoord2f (gl->sh - xepsilon, gl->th - yepsilon);
	VectorMA (origin, width, svright, point);
	qglVertex3fv (point);

	qglEnd ();

	if (a < 1.0f) {
		qglDisable (GL_BLEND);
		qglEnable (GL_ALPHA_TEST);
	}

	qglColor4f (1, 1, 1, 1);
}

void Draw_Pic3D (float x, float y, const char *picn, float scalex, float scaley, float r, float g, float b, float a)
{
	vec3_t origin, org1;
	float mat[3][3];

	origin[0] = x * consoleInfo.surface_width;
	origin[1] = y * consoleInfo.surface_height;
	origin[2] = 0;

	mat[0][0] = consoleInfo.u[0];
	mat[0][1] = consoleInfo.u[1];
	mat[0][2] = consoleInfo.u[2];
	mat[1][0] = consoleInfo.v[0];
	mat[1][1] = consoleInfo.v[1];
	mat[1][2] = consoleInfo.v[2];
	mat[2][0] = consoleInfo.n[0];
	mat[2][1] = consoleInfo.n[1];
	mat[2][2] = consoleInfo.n[2];
	MatrixTransformVector (origin, mat, org1);
	DoDraw_Pic3D (org1,
		consoleInfo.u,
		consoleInfo.v,
		picn,
		consoleInfo.surface_width * scalex,
		consoleInfo.surface_height * scaley,
		r, g, b, a * consoleInfo.alpha);
}

static void DoDraw_StretchPic3D (vec3_t coord, vec3_t u, vec3_t v, int width, int height, const char *pic, float r, float g, float b, float a)
{
	vec3_t svup, svright;
	vec3_t origin, point;
	image_t *gl;

	origin[0] = coord[0];
	origin[1] = coord[1];
	origin[2] = coord[2];

	gl = Draw_FindPic ((char *)pic);
	if (!gl)
	{
		ri.Con_Printf (PRINT_DEVELOPER, "Can't find pic: %s\n", pic);
		return;
	}

	if (scrap_dirty)
		Scrap_Upload ();

	qglColor4f (r, g, b, a);
	GL_Bind (gl->texnum);

	if (a < 1.0f)
	{
		qglDisable (GL_ALPHA_TEST);
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qglEnable (GL_BLEND);
	}

	VectorCopy (v, svup);
	VectorCopy (u, svright);

	qglBegin (GL_QUADS);

	// lower left
	qglTexCoord2f (gl->sl, gl->th);
	VectorCopy (origin, point);
	qglVertex3fv (point);

	// upper left
	qglTexCoord2f (gl->sl, gl->tl);
	VectorMA (origin, height, svup, point);
	qglVertex3fv (point);

	// upper right
	qglTexCoord2f (gl->sh, gl->tl);
	VectorMA (origin, height, svup, point);
	VectorMA (point, width, svright, point);
	qglVertex3fv (point);

	// lower right
	qglTexCoord2f (gl->sh, gl->th);
	VectorMA (origin, width, svright, point);
	qglVertex3fv (point);

	qglEnd ();

	if (a < 1.0f)
	{
		qglDisable (GL_BLEND);
		qglEnable (GL_ALPHA_TEST);
	}

	qglColor4f (1, 1, 1, 1);
}

void Draw_StretchPic3D (float x, float y, float width, float height, const char *picn, float r, float g, float b, float a)
{
	vec3_t origin, org1;
	float mat[3][3];

	origin[0] = x * consoleInfo.surface_width;
	origin[1] = y * consoleInfo.surface_height;
	origin[2] = 0;

	mat[0][0] = consoleInfo.u[0]; mat[0][1] = consoleInfo.u[1]; mat[0][2] = consoleInfo.u[2];
	mat[1][0] = consoleInfo.v[0]; mat[1][1] = consoleInfo.v[1]; mat[1][2] = consoleInfo.v[2];
	mat[2][0] = consoleInfo.n[0]; mat[2][1] = consoleInfo.n[1]; mat[2][2] = consoleInfo.n[2];

	MatrixTransformVector (origin, mat, org1);

	DoDraw_StretchPic3D (org1,
		consoleInfo.u,
		consoleInfo.v,
		(int)(width * consoleInfo.surface_width),
		(int)(height * consoleInfo.surface_height),
		picn,
		r, g, b, a * consoleInfo.alpha);
}

static void DoDraw_TilePic3D (vec3_t coord, vec3_t u, vec3_t v, int width, int height, const char *pic, float r, float g, float b, float a)
{
	vec3_t svup, svright;
	vec3_t origin, point;
	image_t *gl;

	origin[0] = coord[0];
	origin[1] = coord[1];
	origin[2] = coord[2];

	gl = Draw_FindPic ((char *)pic);
	if (!gl)
	{
		ri.Con_Printf (PRINT_DEVELOPER, "Can't find pic: %s\n", pic);
		return;
	}

	if (scrap_dirty)
		Scrap_Upload ();

	qglColor4f (r, g, b, a);
	GL_Bind (gl->texnum);

	if (a < 1.0f)
	{
		qglDisable (GL_ALPHA_TEST);
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qglEnable (GL_BLEND);
	}

	VectorCopy (v, svup);
	VectorCopy (u, svright);

	qglBegin (GL_QUADS);

	// lower left
	VectorCopy (origin, point);
	qglTexCoord2f (point[0] / gl->width, -point[1] / gl->height);
	qglVertex3fv (point);

	// upper left
	VectorMA (origin, height, svup, point);
	qglTexCoord2f (point[0] / gl->width, -point[1] / gl->height);
	qglVertex3fv (point);

	// upper right
	VectorMA (origin, height, svup, point);
	VectorMA (point, width, svright, point);
	qglTexCoord2f (point[0] / gl->width, -point[1] / gl->height);
	qglVertex3fv (point);

	// lower right
	VectorMA (origin, width, svright, point);
	qglTexCoord2f (point[0] / gl->width, -point[1] / gl->height);
	qglVertex3fv (point);

	qglEnd ();

	if (a < 1.0f)
	{
		qglDisable (GL_BLEND);
		qglEnable (GL_ALPHA_TEST);
	}

	qglColor4f (1, 1, 1, 1);
}

void Draw_TilePic3D (float x, float y, float width, float height, const char *picn, float r, float g, float b, float a)
{
	vec3_t origin, org1;
	float mat[3][3];

	origin[0] = x * consoleInfo.surface_width;
	origin[1] = y * consoleInfo.surface_height;
	origin[2] = 0;

	mat[0][0] = consoleInfo.u[0]; mat[0][1] = consoleInfo.u[1]; mat[0][2] = consoleInfo.u[2];
	mat[1][0] = consoleInfo.v[0]; mat[1][1] = consoleInfo.v[1]; mat[1][2] = consoleInfo.v[2];
	mat[2][0] = consoleInfo.n[0]; mat[2][1] = consoleInfo.n[1]; mat[2][2] = consoleInfo.n[2];

	MatrixTransformVector (origin, mat, org1);

	DoDraw_TilePic3D (org1,
		consoleInfo.u,
		consoleInfo.v,
		(int)(width * consoleInfo.surface_width),
		(int)(height * consoleInfo.surface_height),
		picn,
		r, g, b, a * consoleInfo.alpha);
}