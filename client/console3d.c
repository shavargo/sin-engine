
char *_strdate2( char *str );
char *_strtime2( char *str );
//-----------------------------------------------------------------------------
//
//  $Logfile:: /Source/console3d.c                                            $
// $Revision:: 5                                                              $
//   $Author:: Burgermac                                                      $
//     $Date:: 9/27/01 1:11 PM                                                $
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// DESCRIPTION:
// In-game 3D consoles

#include <stdlib.h>
#include "client.h"
#include "ltstyle.h"
#include "cl_bspline.h"
#ifdef _MSC_VER
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif
extern cvar_t *vid_ref;
extern qboolean chat_talk;

static void CON_DPrintf(unsigned int flags, const char *fmt, ...)
{
	va_list argptr;
	char msg[1024];

	(void)flags;
	if (!developer || !developer->value)
		return;

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);
	msg[sizeof(msg)-1] = 0;
	Com_Printf("%s", msg);
}
#define Com_DPrintf CON_DPrintf

//#define CONSOLE_DEBUG 1

#define PICKUP_FADE_TIME 4000
#define MAX_PICKUPS 15
#define AMMO_GRAPH_WIDTH 18.0f
#define AMMO_GRAPH_HEIGHT 32.0f

int con_vidres = 0;
static int client_count = 0;

typedef struct {
	int iconindex;
	int endtime;
	int amount;
} pickup_t;

static qboolean cursor = false;
static int c_teletype = 0;
static int maxwidth;
static char token[MAX_STRING_CHARS];

static byte    *armor_lookup;
static int c_starttime;
static int c_type_starttime;
const char     *menu_in_sound   = "menu_in";
const char     *menu_move_sound = "menu_move";
const char     *menu_out_sound  = "menu_out";
const char     *type_sound      = "menu_type";
float con_centertime_start;
float con_centertime_off;
float con_diatime_start;
float con_diatime_off;
float con_fadetime_off=0;
float con_fadetime_start=0;
float con_fade_r;
float con_fade_g;
float con_fade_b;
float con_fade_a;
int con_fade_direction;
cvar_t         *scr_dialogwidth;
cvar_t         *con_printdialog; /* FS */
int cl_numconsoles = MAX_CONSOLES;
console3d_t cl_consoles[MAX_CONSOLES];
console3d_t mainconsole;
console3d_t genconsole;
console3d_t    *free_consoles, *active_consoles;
static qboolean console3d_init=false;
pickup_t cl_pickups[MAX_PICKUPS];
float savex,savey;
qboolean p_centerx=false,p_centery=false;
qboolean p_left=false,p_right=false;
qboolean p_top=false,p_bottom=false;
cvar_t         *cl_hudfile;
static int teletype_delay = 1;

// MED FIXME FIXME
// move to cl_main.c

static void    *intermission_data = NULL;
static char    *intermission_layout = NULL;
static char intermission_layoutfile[MAX_OSPATH];

typedef enum {UP,DOWN,LEFT,RIGHT} directiontype_t;

static char dialog_string[1024];
static int dialog_icon=0;
static float cl_loadbar_percentage=0;

#define SIGN( x ) x > 0 ? 1 : -1

// This clears out the justify settings
#define CLEAR_JUSTIFY p_centerx=p_centery=p_left=p_right=p_top=p_bottom=false; x=savex; y=savey;

// This will justify a string taking into account the carriage returns
#define JUSTIFY_STRING( a ) JustifyString( p,a,&w,&h )

static void CONSOLE_DialogConsolePrint( const char *str, int icon ); /* FS: Added */

static void JustifyString( console3d_t *p,const char *a,float *Outw,float *Outh )
{
	int i;
	float w,h;
	w = 0;
	h = 1;
	for (i=0; i<strlen( a ); i++) {
		if (a[i] == '\n') {
			h++;
			maxwidth = (int) (__max( w,maxwidth ));
			w = 0;
		} else {
			w++;
		}
	}
	w = __max( w,maxwidth );
	w *= (1.0f / p->columns);
	h *= (1.0f / p->rows);
	Outw[0] = w;
	Outh[0] = h;
}

// This will justify an object
#define JUSTIFY Justify( p,&x,&y,w,h )

static void Justify( console3d_t *p,float *Outx,float *Outy,float w,float h )
{
	float x,y;
	x = Outx[0];
	y = Outy[0];
	savex = x;
	savey = y;
	if (p_centerx)
		x = (1.0f - w) / 2.0f;
	else if (p_right)
		x = 1.0f - w;
	else if (p_left)
		x = 0;
	else
		x /= p->virtual_width;

	if (p_centery)
		y = (1.0f - h) / 2.0f;
	else if (p_top)
		y = 1.0f - h;
	else if (p_bottom)
		y = 0;
	else
		y /= p->virtual_height;
	Outx[0] = x;
	Outy[0] = y;
}

// This takes care of animated and blinking cursors
#define CURSOR Cursor( p,x,y,h,r,g,b,a )

static void Cursor( console3d_t *p,float x,float y,float h,float r,float g,float b,float a )
{
	if (cursor && (p->menu3d[p->current_menu_level]->cursor.numframes != 0)) {
		char animframe[MAX_OSPATH];
		char *name = p->menu3d[p->current_menu_level]->cursor.name;
		int numframes = p->menu3d[p->current_menu_level]->cursor.numframes;
		int f = (int)((cls.realtime / 100)%numframes) + 1;
		float w2,h2;
		w2 = p->menu3d[p->current_menu_level]->cursor.width  / p->virtual_width;
		h2 = p->menu3d[p->current_menu_level]->cursor.height / p->virtual_height;
		sprintf( animframe,"%s%d",name,f );
		re.DrawPic3D( x-w2,y-(h2/2.0f)+(h/2.0f),animframe,p->oovw,p->oovh,r,g,b,a );
	} else {
		if (( cursor && (cls.realtime / 100) % p->blinkrate)) {
			re.DrawChar3D( x-(1.0f / p->columns),y,13,p->rows,p->columns,r,g,b,a );
			re.FlushChar3D(r, g, b, a); /* FS */
		}
	}
}

// This takes care of typing strings
#define TSTRING TString( p )

static void TString( console3d_t *p )
{
	if (c_teletype) {
		p->type_index = (teletype_delay*(cls.realtime - c_type_starttime)/100);
		if (p->type_index < strlen( token )) {
			if ( ( token[p->type_index] != ' ' ) && ( c_teletype == 1 ) ) {
				CL_RandomLocalSound( type_sound );
			}
			if (p->type_index != 0) {
				token[p->type_index-1] = 11;
				token[p->type_index] = 0;
			} else {
				token[0] = 0;
			}
		}
	}
}

static void MenuOnly( console3d_t *p )
{
	if ((p != cls.mainconsole) && (p != cls.genconsole)) {
		Com_Error( ERR_FATAL, "CONSOLE_DrawLayout: drawplmodel is not supported in game consoles\n" );
	}
}

/*
   3D Console is a generic name for 2D things that are drawn on
   3D surfaces in the game.  For instance a "3D console" can
   have a combination of text strings, graphics, menus, scrolling
   consoles (that is similar to the Quake main console) and
   other things.

   Consoles can have 2 flags set that tells the functions what to
   draw.  If the scrollcon flag is set, then there is a scrolling,
   word-wrapping, user inputable quake style console on the surface.

   If the menucon flag is set, then there is a menu definition that
   is drawn in similar style to the main game menu system.

   Layout strings are used to draw strings and graphics on the surface.
   Each 3D console has a layout string that can be used to display
   just about anything the user wants.  If menucon is set, then each
   menu item has multiple layout strings and which one is drawn depends
   on whether or not that item is selected.

   The user input is also redirected when a console is used.  A message
   from the server will tell this function how to change the input.
   3D consoles can have 2 types of input console3d or menu3d.  Each
   one has its own input handler and the script can change the focus
   dynamically.
*/

/*
==========================================================
UTILITIES
==========================================================
*/

/*
========
CONSOLE_strdup
========
*/
char *CONSOLE_strdup( const char *str, console3d_t *p )
{
	char *newstr;

	newstr = (char*)Z_TagMalloc( (int)strlen( str ) + 1, TAG_CONSOLE_BASE+p->console_number );
	strcpy( newstr, str );
	return newstr;
}

/*
=========
conva
=========
*/
static const char *
 __attribute__((__format__(__printf__,1,2)))
conva( const char *format, ... )
{
	va_list argptr;
	static char string[1024];

	va_start( argptr, format );
	Q_vsnprintf( string, sizeof(string), format,argptr );
	va_end( argptr );
	string[sizeof(string)-1] = 0;

	return string;
}

/*
===========================================================
                INITIALIZATION FUNCTIONS
===========================================================
*/

void MENU_Reload_f( void )
{
	MENU_InitPlayerConfig();
	MENU_InitDeathmatchMapsConfig();
	if ( viddef.width >= 640 && viddef.height >= 480 )
	{
		CONSOLE_LoadMenu( "maincon menus/main.mnu" );
	}
	else
	{
		CONSOLE_LoadMenu( "maincon menus/main_alt.mnu" );
	}
}

/*
================================================================
CONSOLE_Init

Sets up all the static consoles and creates the utility commands
================================================================
*/

void CONSOLE_Init( void )
{
	int i;

	free_consoles = &cl_consoles[0];
	active_consoles = NULL;

	for (i=0; i<cl_numconsoles; i++)
		cl_consoles[i].next = &cl_consoles[i+1];

	cl_consoles[cl_numconsoles-1].next = NULL;
	cls.currentconsole = 0;


	// Load in the appropriate menu

	if (viddef.width >= 640 && viddef.height >= 480)
	{
		cls.mainconsole = CONSOLE_Create( "maincon 0 640 480 60 80 1.0",false,true );
		CONSOLE_LoadMenu( "maincon menus/main.mnu" );
		Cvar_SetValue( "scr_dialogwidth", 30 );
		con_vidres = CON_HIRES;
	}
	else
	{
		cls.mainconsole = CONSOLE_Create( "maincon 0 320 240 30 40 1.0",false,true );
		CONSOLE_LoadMenu( "maincon menus/main_alt.mnu" );
		Cvar_SetValue( "scr_dialogwidth", 22 );
		con_vidres = CON_LORES;
	}

	cls.genconsole = CONSOLE_Create( "gencon 50 640 480 60 80 1.0",false,true );

	console3d_init = true;

	CL_InitializeBSplines();
}

/*
===========================================================
CONSOLE_ConCheckResize

Checks to see if the scrolling console has been resized and
reformats the text as necessary.
===========================================================
*/
static void CONSOLE_ConCheckResize( console3d_t *p )
{
	int i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char tbuf[CON3D_TEXTSIZE];

	width = p->columns; //p->virtual_width / p->font_width;

	if (width == p->con.linewidth)
		return;

	if (width < 1)          // video hasn't been initialized yet
	{
		width = 38;
		p->con.linewidth = width;
		p->con.totallines = CON3D_TEXTSIZE / p->con.linewidth;
		memset ( p->con.text, ' ', CON3D_TEXTSIZE );
	}
	else
	{
		oldwidth = p->con.linewidth;
		p->con.linewidth = width;
		oldtotallines = p->con.totallines;
		p->con.totallines = CON3D_TEXTSIZE / p->con.linewidth;
		numlines = oldtotallines;

		if (p->con.totallines < numlines)
			numlines = p->con.totallines;

		numchars = oldwidth;

		if (p->con.linewidth < numchars)
			numchars = p->con.linewidth;

		memcpy( tbuf, p->con.text, CON3D_TEXTSIZE );
		memset ( p->con.text, ' ', CON3D_TEXTSIZE );

		for (i=0; i<numlines; i++)
		{
			for (j=0; j<numchars; j++)
			{
				p->con.text[(p->con.totallines - 1 - i) * p->con.linewidth + j] =
					tbuf[((p->con.current - i + oldtotallines) %
					      oldtotallines) * oldwidth + j];
			}
		}
	}

	p->con.current = p->con.totallines - 1;
	p->con.display = p->con.current;
}


/*
===============
CONSOLE_InitCon

Initializes the console edit lines and also checks if the console
has been resized and reformats the text if necessary.
===============
*/
static void CONSOLE_InitCon( console3d_t *p )
{
	p->con.initialized = true;
	CONSOLE_ConCheckResize( p );
}


/*
===========================================================
                DRAWING FUNCTIONS
===========================================================
*/

/*
===============
DrawSlider3D
===============
*/
static void DrawSlider3D( float x, float y, menuslider3d_t *s, console3d_t *p, float r, float g, float b, float a )
{
	float orgx;
	float width;
	int i;

	orgx = x;
	width  = p->virtual_width  / p->columns;

	s->range = ( s->curvalue - s->minvalue ) / ( float ) ( s->maxvalue - s->minvalue );

	if ( s->range < 0)
		s->range = 0;
	if ( s->range > 1)
		s->range = 1;

	// Draw the left endcap
	re.DrawChar3D( x/p->virtual_width,y/p->virtual_height,128,p->rows,p->columns,r,g,b,a );
	x += width;

	// Draw the middle stuff
	for ( i = 0; i < s->length; i++ )
	{
		re.DrawChar3D ( x/p->virtual_width,y/p->virtual_height,129,p->rows,p->columns,r,g,b,a );
		x += width;
	}

	// Draw the right endcap
	re.DrawChar3D ( x/p->virtual_width,y/p->virtual_height,130,p->rows,p->columns,r,g,b,a );

	// Draw the cursor.
	x = orgx;
	x += (s->length+1)*s->range*width;
	re.DrawChar3D ( x/p->virtual_width,y/p->virtual_height,131,p->rows,p->columns,r,g,b,a );
	re.FlushChar3D(r,g,b,a); /* FS */
}

/*
============
DrawString3D
============
*/
static void DrawString3D( float x, float y, const char *s, console3d_t *p, float r, float g, float b, float a )
{
	float width,height;
	float orgx = x;
	const char *s2 = s;
	width  = 1.0f / p->columns;
	height = 1.0f / p->rows;

	while (*s2)
	{
		if (*s2 == '\n')
			y += height;
		s2++;
	}

	while (*s)
	{
		re.DrawChar3D( x,y,*s,p->rows,p->columns,r,g,b,a );
		if (*s == '\n')
		{
			x = orgx;
			y -= height;
		}
		else
		{
			x += width;
		}
		s++;
	}

	re.FlushChar3D(r,g,b,a); /* FS */
}

/*
============
DrawAltString3D
============
*/
static void DrawAltString3D( float x, float y, const char *s, console3d_t *p,float r, float g, float b, float a )
{
	float width,height;
	float orgx = x;

	width  = 1.0f  / p->columns;
	height = 1.0f  / p->rows;

	while (*s)
	{
		re.DrawChar3D (x,y,*s^0x80,p->rows,p->columns,1.0f,1.0f,1.0f,1.0f);
		if (*s == '\n')
		{
			x = orgx;
			y += height;
		}
		else
			x += width;
		s++;
	}
	re.FlushChar3D(1.0f,1.0f,1.0f,1.0f); /* FS */
}

/*
===========
DrawField3D
===========
*/
static void DrawField3D( float x, float y, menufield3d_t *f, console3d_t *p, float r, float g, float b, float a, qboolean selected )
{
	float orgx,orgy;
	float width,height;
	char tempbuffer[128] = "";
	int i,offset;

	orgx = x;
	orgy = y;

	width  = 1.0f / p->columns;
	height = 1.0f / p->rows;

	// Draw the left endcap
	re.DrawChar3D( x,y+height,18,p->rows,p->columns,r,g,b,a );
	re.DrawChar3D( x,y,24,p->rows,p->columns,r,g,b,a );
	x+=width;
	// Draw the middle stuff
	for ( i = 0; i < f->visible_length+1; i++ )
	{
		re.DrawChar3D ( x,y+height,19,p->rows,p->columns,r,g,b,a );
		re.DrawChar3D ( x,y,25,p->rows,p->columns,r,g,b,a );
		x += width;
	}
	// Draw the right endcap
	re.DrawChar3D( x,y+height,20,p->rows,p->columns,r,g,b,a );
	re.DrawChar3D( x,y,26,p->rows,p->columns,r,g,b,a );

	// Draw the buffer on top of it.
	strncpy( tempbuffer, f->buffer + f->visible_offset, f->visible_length );
	DrawString3D( orgx+width,orgy+(height/2),tempbuffer,p,r,g,b,a );

	if (selected)
	{
		if ( f->visible_offset )
			offset = f->visible_length;
		else
			offset = f->cursor;

		orgx += width;
		if ( ( ( int ) ( Sys_Milliseconds() / 250 ) ) & 1 )
		{
			re.DrawChar3D( orgx + (offset * width),orgy+(height/2),11,p->rows,p->columns,r,g,b,a );
		}
		else
		{
			re.DrawChar3D( orgx + (offset * width),orgy,' ',p->rows,p->columns,r,g,b,a );
		}
	}
	re.FlushChar3D(r,g,b,a); /* FS */
}

/*
=============
DrawBoxString
=============
*/
static void DrawBoxString3D( float x, float y, const char *buffer, console3d_t *p, float r, float g, float b, float a )
{
	float orgx,orgy;
	float width,height;
	int i;

	orgx = x;
	orgy = y;

	width  = 1.0f / p->columns;
	height = 1.0f / p->rows;

	// Draw the left endcap
	re.DrawChar3D( x,y+height,18,p->rows,p->columns,r,g,b,a );
	re.DrawChar3D( x,y,24,p->rows,p->columns,r,g,b,a );
	x+=width;
	// Draw the middle stuff
	for ( i = 0; i < strlen( buffer ); i++ )
	{
		re.DrawChar3D ( x,y+height,19,p->rows,p->columns,r,g,b,a );
		re.DrawChar3D ( x,y,25,p->rows,p->columns,r,g,b,a );
		x += width;
	}
	// Draw the right endcap
	re.DrawChar3D( x,y+height,20,p->rows,p->columns,r,g,b,a );
	re.DrawChar3D( x,y,26,p->rows,p->columns,r,g,b,a );

	// Draw the buffer on top of it.
	DrawString3D( orgx+width,orgy+(height/2),buffer,p,r,g,b,a );
	re.FlushChar3D(r,g,b,a); /* FS */
}

/*
====================
GetBoxTextDimensions
====================
*/
static void GetBoxTextDimensions( const char *buffer, int *linewidth, int *lines )
{
	const char *s = buffer;
	int maxlinewidth = 0;

	*linewidth = 0;
	*lines = 1;

	// Scan the buffer for it's maxwidth and the number of lines it has.
	while (*s)
	{
		if (*s == '\n')
		{
			(*lines)++;
			*linewidth = __max( *linewidth, maxlinewidth );
			maxlinewidth = 0;
		}
		else
		{
			maxlinewidth++;
		}
		s++;
	}
	*linewidth = __max( *linewidth, maxlinewidth );
}

/*
=============
DrawBoxText3D - This one draws a box on a string that has carriage returns in it
=============
*/
static void DrawBoxText3D( float x, float y, const char *buffer, console3d_t *p, float r, float g, float b, float a )
{
	float cx, cy;
	int n;
	float width  = 1.0f / p->columns;
	float height = 1.0f / p->rows;
	const char *s = buffer;
	int linewidth = 0;
	int maxlinewidth = 0;
	int lines = 1;
	float x1 = 0;

	cx = x;
	cy = y;

	// Scan the buffer for it's maxwidth and the number of lines it has.
	while (*s) {
		if (*s == '\n') {
			lines++;
			linewidth = __max( linewidth, maxlinewidth );
			maxlinewidth = 0;
		} else {
			maxlinewidth++;
		}
		s++;
	}
	linewidth = __max( linewidth, maxlinewidth );

	// draw left bottom
	re.DrawChar3D ( cx,cy,7,p->rows,p->columns,r,g,b,a );
	// draw left side
	for (n = 0; n < lines; n++) {
		cy += height;
		re.DrawChar3D ( cx,cy,4,p->rows,p->columns,r,g,b,a );
	}
	// draw left top
	re.DrawChar3D ( cx,cy+height,1,p->rows,p->columns,r,g,b,a );

	// draw middle stuff
	cx += width;
	while (linewidth > 0) {
		cy = y;
		// draw middle bottom
		re.DrawChar3D ( cx,cy,8,p->rows,p->columns,r,g,b,a );
		// draw middle
		for (n = 0; n < lines; n++) {
			cy += height;
			re.DrawChar3D ( cx,cy,5,p->rows,p->columns,r,g,b,a );
		}
		// draw middle top
		re.DrawChar3D ( cx,cy+height,2,p->rows,p->columns,r,g,b,a );
		linewidth -= 1;
		cx += width;
	}

	s = buffer;
	while (*s) {
		if (*s == '\n') {
			x1 = 0;
			cy -= height;
			s++;
			continue;
		}
		re.DrawChar3D ( x+width+x1,cy,*s,p->rows,p->columns,r,g,b,a );
		x1 += width;
		s++;
	}
	cy = y;

	// draw right bottom
	re.DrawChar3D ( cx,cy,9,p->rows,p->columns,r,g,b,a );

	// draw right side
	for (n = 0; n < lines; n++) {
		cy += height;
		re.DrawChar3D ( cx,cy,6,p->rows,p->columns,r,g,b,a );
	}
	// draw right top
	re.DrawChar3D ( cx,cy+height,3,p->rows,p->columns,r,g,b,a );
	re.FlushChar3D(r,g,b,a); /* FS */
}


/*
=========
DrawBox3D
=========
*/
static void DrawBox3D( float x, float y, int linewidth, int lines, console3d_t *p, float r,float g,float b,float a )
{
	float cx, cy;
	int n;
	float width  = 1.0f / p->columns;
	float height = 1.0f / p->rows;

	cx = x;
	cy = y;

	// draw left bottom
	//re.DrawChar3D (cx,cy,7,p->rows,p->columns,r,g,b,a);
	re.DrawChar3D ( cx,cy,24,p->rows,p->columns,r,g,b,a );

	// draw left side
	for (n = 0; n < lines; n++)
	{
		cy += height;
		//re.DrawChar3D (cx,cy,4,p->rows,p->columns,r,g,b,a);
		re.DrawChar3D ( cx,cy,21,p->rows,p->columns,r,g,b,a );
	}
	// draw left top
	//re.DrawChar3D (cx,cy+height,1,p->rows,p->columns,r,g,b,a);
	re.DrawChar3D ( cx,cy+height,18,p->rows,p->columns,r,g,b,a );

	// draw middle stuff
	cx += width;
	while (linewidth > 0)
	{
		cy = y;
		// draw middle bottom
		//re.DrawChar3D (cx,cy,8,p->rows,p->columns,r,g,b,a);
		re.DrawChar3D ( cx,cy,25,p->rows,p->columns,r,g,b,a );

		// draw middle
		for (n = 0; n < lines; n++)
		{
			cy += height;
			//re.DrawChar3D (cx,cy,5,p->rows,p->columns,r,g,b,a);
			re.DrawChar3D ( cx,cy,22,p->rows,p->columns,r,g,b,a );
		}
		// draw middle top
		//re.DrawChar3D (cx,cy+height,2,p->rows,p->columns,r,g,b,a);
		re.DrawChar3D ( cx,cy+height,19,p->rows,p->columns,r,g,b,a );

		linewidth -= 1;
		cx += width;
	}

	cy = y;
	// draw right bottom
	//re.DrawChar3D (cx,cy,9,p->rows,p->columns,r,g,b,a);
	re.DrawChar3D ( cx,cy,26,p->rows,p->columns,r,g,b,a );

	// draw right side
	for (n = 0; n < lines; n++)
	{
		cy += height;
		//re.DrawChar3D (cx,cy,6,p->rows,p->columns,r,g,b,a);
		re.DrawChar3D ( cx,cy,23,p->rows,p->columns,r,g,b,a );
	}
	// draw right top
	//re.DrawChar3D (cx,cy+height,3,p->rows,p->columns,r,g,b,a);
	re.DrawChar3D ( cx,cy+height,20,p->rows,p->columns,r,g,b,a );
	re.FlushChar3D(r,g,b,a); /* FS */
}



#define STAT_MINUS      10  // num frame for '-' stats digit
const char *con_nums[2][11] =
{
	{"num_0", "num_1", "num_2", "num_3", "num_4", "num_5",
	 "num_6", "num_7", "num_8", "num_9", "num_minus"},
	{"anum_0", "anum_1", "anum_2", "anum_3", "anum_4", "anum_5",
	 "anum_6", "anum_7", "anum_8", "anum_9", "anum_minus"}
};

#define ICON_WIDTH  24
#define ICON_HEIGHT 24
#define CHAR_WIDTH  16
#define  CHAR_HEIGHT 24
#define ICON_SPACE  8

/*
=================
CONSOLE_DrawStat
=================
*/
static void CONSOLE_DrawStat( float x, float y, int color, int width, int value, console3d_t *p )
{
	char num[16], *ptr;
	int l;
	int frame;
	float r=1.0f,g=1.0f,b=1.0f,a=1.0f;
	float xwidth = CHAR_WIDTH / p->virtual_width;
	int delta=0;

	if (width < 1)
		return;

	// draw number string
	if (width > 5)
		width = 5;

	//SCR_AddDirtyPoint (x, y);
	//SCR_AddDirtyPoint (x+width*CHAR_WIDTH+2, y+23);

	Com_sprintf ( num, sizeof(num), "%i", value );

	l = (int)strlen( num );

	if (l < width)
	{
		delta = width - l;
	}

	if (l > width)
	{
		l = width;
	}

	x += (xwidth * delta) / 2.0f;

	ptr = num;
	while (*ptr && l)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';
		re.DrawPic3D( x,y,con_nums[color][frame],p->oovw,p->oovh,r,g,b,a );
		x += xwidth;
		ptr++;
		l--;
	}
}

/*
==================================================================
CONSOLE_DrawInput3D

Draws the user input line on the console with the blinking cursor.
==================================================================
*/
static void CONSOLE_DrawInput3D( console3d_t *p )
{
	int i,x;
	char        *text;
	float width;

	width  = p->virtual_width  / p->columns;

	text = p->KeyLinesPtr[p->edit_line];

	// add the cursor frame
	text[p->key_linepos] = 10+((int)(cls.realtime>>8)&1);

	// fill out remainder with spaces
	for (i=p->key_linepos+1; i< p->con.linewidth; i++)
		text[i] = ' ';

	//	prestep if horizontally scrolling
	if (p->key_linepos >= p->con.linewidth)
		text += 1 + p->key_linepos - p->con.linewidth;

	for (x=0; x<p->con.linewidth; x++)
	{
		re.DrawChar3D((x*width)/p->virtual_width,0,text[x],p->rows,p->columns,p->r,p->g,p->b,p->a );
	}
	re.FlushChar3D(p->r,p->g,p->b,p->a); /* FS */

	// remove cursor
	p->KeyLinesPtr[p->edit_line][p->key_linepos] = 0;
}

/*
=======================
CONSOLE_DrawFullAmmoBar
=======================
*/
static void CONSOLE_DrawFullAmmoBar( float x, float y, const char *s, console3d_t *p )
{
	const char *s2;
	const char *token;

	float wx,w,h;
	int i;
	float red,green,blue,alpha;
	float w2,h2;
	float x2,y2;
	float max,ammo_per_shot;
	int value,ammo_num;

	float ammo_bar_width   = 5.0f  / p->virtual_width;
	float ammo_bar_xoffset = 12.0f / p->virtual_width;
	float ammo_bar_yoffset = 1.0f  / p->virtual_height;
	float ammo_bar_w       = AMMO_GRAPH_WIDTH  / p->virtual_width;
	float ammo_bar_h       = AMMO_GRAPH_HEIGHT / p->virtual_height;

	if (scr_viewsize->value > 100)
		return;

	// this should fix crashes in release
	s2 = s;
	wx = ammo_bar_w;
	w  = ammo_bar_w * 10;
	h  = ammo_bar_h;
	JUSTIFY;
	w2 = ammo_bar_width;
	x2  = x + ammo_bar_xoffset;
	y2  = y + ammo_bar_yoffset;
	for (i=0; i<10; i++)
	{
		h2  = 27;
		red=0,green=1.0f,blue=0,alpha=1.0f;

		token = COM_Parse( &s2 );
		ammo_num = atoi( token );
		token = COM_Parse( &s2 );
		max = Sinatof( token );
		token = COM_Parse( &s2 );
		ammo_per_shot = Sinatof( token );

		if (ammo_num)
		{
			value = cl.frame.playerstate.stats[ammo_num];
			if (value <= 0)
				value = 0;
			else if (value < max/4)
				red = 1.0f, green = 0, blue = 0;
			else if (value < max/2)
				red = 1.0f, green = 1.0f, blue = 0;
			else if (value > max)
			{
				alpha = 1.0f;
				value = max;
			}
			value /= (float)ammo_per_shot;
		}
		else
		{
			value = max;
			red=0; green=0; blue=1.0f;
		}

		h2 *= (float)value / max;
		h2 /= p->virtual_height;

		re.DrawFill3D( x2,y2,w2,h2,red,green,blue,alpha );
		re.DrawPic3D( x,y,"hud_ammo_graph",p->oovw,p->oovh,1.0f,1.0f,1.0f,1.0f );
		if (i == 9)
			re.DrawChar3D( x,y,'0',p->rows,p->columns,0.0f,1.0f,1.0f,1.0f );
		else
			re.DrawChar3D( x,y,i+49,p->rows,p->columns,0.0f,1.0f,1.0f,1.0f );

		x += wx;
		x2 += wx;
	}
	re.FlushChar3D(0.0f,1.0f,1.0f,1.0f); /* FS */
	CLEAR_JUSTIFY;
}

static void CONSOLE_DrawNotify( float x, float y, int lines, console3d_t *p, float r, float g, float b, float a )
{

	char          *text;
	int i,j;
	int time;
	float width,height;
	float orgx;

	extern cvar_t  *con_notifytime;

	width  = 1.0f / p->columns;
	height = 1.0f / p->rows;

	x /= p->virtual_width;
	y /= p->virtual_height;

	orgx = x;

	for ( i = con.current-lines + 1; i <= con.current; i++ )
	{
		if (i < 0)
			continue;
		time = con.times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;
		time = cls.realtime - time;
		if (time > con_notifytime->value*1000)
			continue;
		text = con.text + (i % con.totallines)*con.linewidth;

		for ( j = 0; j < con.linewidth; j++ )
		{
			re.DrawChar3D( x,y,text[j],p->rows,p->columns,r,g,b,a );
			x += width;
		}

		x = orgx;
		y -= height;
	}
	re.FlushChar3D(r,g,b,a); /* FS */
}

static void CONSOLE_DrawMessage( float x, float y, int linewidth, console3d_t *p, float r, float g, float b, float a )
{
	int skip;
	int i;
	char    *s;
	float width  = 1.0f / p->columns;

	x /= p->virtual_width;
	y /= p->virtual_height;

	if (cls.key_dest == key_message)
	{
		if (chat_talk)
		{
			DrawString3D ( x,y,"talk:",p,r,g,b,a );
			skip = 6;
		}
		else if (chat_team)
		{
			DrawString3D ( x,y,"say_team:",p,r,g,b,a );
			skip = 11;
		}
		else
		{
			DrawString3D ( x,y,"say:",p,r,g,b,a );
			skip = 5;
		}

		s = chat_buffer;

		if ( chat_bufferlen > ( linewidth - ( skip + 1 ) ) )
			s += chat_bufferlen - ( linewidth - ( skip + 1 ) );

		x += skip * width;
		i=0;
		while(s[i])
		{
			re.DrawChar3D( x,y,s[i],p->rows,p->columns,r,g,b,a );
			x += width;
			i++;
		}
		re.DrawChar3D( x,y,10+((cls.realtime>>8)&1),p->rows,p->columns,r,g,b,a );
	}
	re.FlushChar3D(r,g,b,a); /* FS */
}
/*
======================
CONSOLE_DrawWeaponList
======================
*/
static void CONSOLE_DrawWeaponList( float x, float y, const char *s, console3d_t *p )
{
	float w,h;
	int i;
	float wx = 19.0f / p->virtual_width;

	if (scr_viewsize->value > 100)
		return;

	w = wx * 10;
	h = 16.0f;
	JUSTIFY;
	// this should fix crashes in release
	for (i=1; i<=10; i++)
	{
		if (i == 10)
		{
			re.DrawPic3D( x,y, "snum_0" ,p->oovw,p->oovh,1.0f,1.0f,1.0f,1.0f );
		}
		else
		{
			re.DrawPic3D( x,y,conva( "snum_%d",i ),p->oovw,p->oovh,1.0f,1.0f,1.0f,1.0f );
		}
		x += wx;
	}
	CLEAR_JUSTIFY;
}

/*
==========================================
DrawLayout3D

Draws the layout string that is passed in.
==========================================
*/

//#pragma options opt = local
#ifdef _MSC_VER
#pragma optimize("g", off)
#endif

void DrawLayout3D( const char *s, console3d_t *p, int itemnum )
{
	float x=0;
	float y=0;
	float w,h,w2;
	float r=1.0f,g=1.0f,b=1.0f,a=1.0f;
	qboolean ifstatement = false;
	int i;
	char temp[1024];
	int direction;
	item3d_t *item=0;

	maxwidth = 0;
	cursor = false;
	c_teletype = 0;
	p_centerx=false,p_centery=false;
	p_left=false,p_right=false;
	p_top=false,p_bottom=false;

	if ( p->menu3d && p->menu3d[p->current_menu_level] && itemnum >= 0 )
	{
		item = p->menu3d[p->current_menu_level]->item[itemnum];
	}

	if ( item && item->fade )
	{
		float fadetime;
		float delta;

		if ( c_starttime + (item->fade_delay*1000) > cls.realtime )
		{
			a = 0;
		}
		else
		{
			delta = (float)(cls.realtime - (c_starttime+(item->fade_delay*1000))) / 1000.0f;
			fadetime = delta / item->fade;

			if ( fadetime < 0.2 )
				a = fadetime / 0.2f;

			if ( fadetime > 0.8 )
				a = ( 1 - fadetime ) / 0.2f;
		}
	}

	while (s)
	{
		strcpy( token, COM_Parse( &s ) );
		if (!strcmp( token, "if" )) // If stat number
		{
			int value;
			strcpy( token, COM_Parse ( &s ));
			value = cl.frame.playerstate.stats[atoi( token )];
			if (!value)
			{       // skip to endif or else
				while (s && strcmp( token, "endif" ) && strcmp( token, "else" ))
				{
					strcpy( token, COM_Parse ( &s ));
				}
			}
			else
			{
				ifstatement = true;
			}
			continue;
		}
		if (!strcmp( token,"else" ))
		{
			if (ifstatement)
			{
				// We were in an if statement, so now skip the else part
				strcpy( token, COM_Parse ( &s ));
				while (s && strcmp( token, "endif" ))
				{
					strcpy( token, COM_Parse ( &s ));
				}
				ifstatement = false;
			}
			else
			{
				ifstatement = true;
			}
			continue;
		}
		if (!strcmp( token, "ifdm" )) // If deathmatch
		{
			if (strcmp( cl.configstrings[CS_STATUSBAR], "DEATHMATCH" ))
			{       // skip to endif
				while (s && strcmp( token, "endif" ) && strcmp( token, "else" ))
				{
					strcpy( token, COM_Parse ( &s ));
				}
			}
			else
			{
				ifstatement = true;
			}
			continue;
		}
		if (!strcmp( token, "ifsoft" )) // If driver is software
		{
			if (strcmp( vid_ref->string, "soft" ))
			{       // skip to endif
				while (s && strcmp( token, "endif" ) && strcmp( token, "else" ))
				{
					strcpy( token, COM_Parse ( &s ));
				}
			}
			else
			{
				ifstatement = true;
			}
			continue;
		}
		if (!strcmp( token, "ifcp" )) // If checkpoints
		{
			if(!strcmp( cl.configstrings[CS_CHECKPOINTS], "NONE" ))
			{  // skip to endif
				while (s && strcmp( token, "endif" ) && strcmp( token, "else" ))
				{
					strcpy( token, COM_Parse ( &s ));
				}
			}
			else
			{
				ifstatement = true;
			}
			continue;
		}
		if (!strcmp( token, "ifpowerup" )) // If a powerup is active
		{
			int powerup = atoi( COM_Parse( &s ));
			if (!(cl.frame.playerstate.stats[STAT_POWERUPTYPE] == powerup))
			{       // skip to endif
				while (s && strcmp( token, "endif" ) && strcmp( token, "else" ))
				{
					strcpy( token, COM_Parse ( &s ));
				}
			}
			else
			{
				ifstatement = true;
			}
			continue;
		}
		if (!strcmp( token, "ifdialog" )) // If dialog active
		{
			con_diatime_off -= cls.renderFrametime;
			if (con_diatime_off <= 0)
			{
				while (s && strcmp( token, "endif" ) && strcmp( token, "else" ))
				{
					strcpy( token, COM_Parse ( &s ));
				}
			}
			else
			{
				ifstatement = true;
			}
			continue;
		}
		if (!strcmp( token, "jl" )) // justify left
		{
			p_left = true;
			continue;
		}
		if (!strcmp( token, "jr" )) // justify right
		{
			p_right = true;
			continue;
		}
		if (!strcmp( token, "jt" )) // justify top
		{
			p_top = true;
			continue;
		}
		if (!strcmp( token, "jb" )) // justify bottom
		{
			p_bottom = true;
			continue;
		}
		if (!strcmp( token, "jcx" )) // justify center in x
		{
			p_centerx = true;
			continue;
		}
		if (!strcmp( token, "jcy" )) // justify center in y
		{
			p_centery = true;
			continue;
		}
		if (!strcmp( token, "xl" )) // xoffset from the left of surface
		{
			strcpy( token, COM_Parse( &s ) );
			x = Sinatof( token );
			continue;
		}
		if (!strcmp( token, "xr" )) // xoffset from the right of surface
		{
			strcpy( token, COM_Parse( &s ) );
			x = p->virtual_width + Sinatof( token );
			continue;
		}
		if (!strcmp( token, "oxl" )) // xoffset from the left of surface + a screen offset
		{
			strcpy( token, COM_Parse( &s ) );
			x = Sinatof( token ) + p->offsetx;;
			continue;
		}
		if (!strcmp( token, "oxr" )) // xoffset from the right of surface + a screen offset
		{
			strcpy( token, COM_Parse( &s ) );
			x = p->virtual_width + Sinatof( token ) - p->offsetx;
			continue;
		}
		if (!strcmp( token, "xv" )) // offset from the center of the surface in the x-axis
		{
			strcpy( token, COM_Parse( &s ) );
			x = p->virtual_width / 2.0f + Sinatof( token );
			continue;
		}
		if (!strcmp( token, "yb" )) // offset from the bottom of surface
		{
			strcpy( token, COM_Parse( &s ) );
			y = Sinatof( token );
			continue;
		}
		if (!strcmp( token, "yt" )) // offset from the top of surface
		{
			strcpy( token, COM_Parse( &s ) );
			y = p->virtual_height + Sinatof( token );
			continue;
		}
		if (!strcmp( token, "oyb" )) // offset from the bottom of surface + a screen offset
		{
			strcpy( token, COM_Parse( &s ) );
			y = Sinatof( token ) + p->offsety;
			continue;
		}
		if (!strcmp( token, "oyt" )) // offset from the top of surface + a screen offset
		{
			strcpy( token, COM_Parse( &s ) );
			y = p->virtual_height + Sinatof( token ) - p->offsety;
			continue;
		}
		if (!strcmp( token, "yv" )) // offset from the center of the surface in the y-axis
		{
			strcpy( token, COM_Parse( &s ) );
			y = p->virtual_height / 2.0f + Sinatof( token );
			continue;
		}
		if (!strcmp( token, "fc" )) // Set the foreground color
		{
			r = Sinatof( COM_Parse( &s ));
			g = Sinatof( COM_Parse( &s ));
			b = Sinatof( COM_Parse( &s ));
			a = Sinatof( COM_Parse( &s ));
			continue;
		}
		if (!strcmp( token, "br" )) // Set the blinking rate
		{
			p->blinkrate = atoi( COM_Parse( &s ));
		}
		if (!strcmp( token, "cursor" )) // draw a cursor symbol
		{
			cursor = ~cursor;
			continue;
		}
		if (!strcmp( token, "fadein" )) // Fade this item into being
		{
			float max;

			max = Sinatof( COM_Parse( &s ));

			a = ((float)(cls.realtime - c_starttime)/1000.0f) / max;
			if (a > 1 )
				a = 1;
			continue;
		}
		if (!strcmp( token, "fade" )) // Fade this item into being
		{
			float max;

			max = Sinatof( COM_Parse( &s ));

			a = ((float)(cls.realtime - c_starttime)/1000.0f) / 1.0f;
			if (a > max || !cl.refresh_prepped )
				a = max;
			continue;
		}
		if (!strcmp( token, "border" )) // specify a border to draw
		{
			w = Sinatof( COM_Parse( &s )) / p->virtual_width;
			h = Sinatof( COM_Parse( &s )) / p->virtual_height;

			JUSTIFY;
			re.DrawFill3D( x,y,w,p->oovh,r,g,b,a );
			re.DrawFill3D( x,y,p->oovw,h,r,g,b,a );
			re.DrawFill3D( x+w,y,p->oovw,h,r,g,b,a );
			re.DrawFill3D( x,y+h,w,p->oovw,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "box" )) // specify a box to draw
		{
			int linewidth, lines;

			strcpy( token, COM_Parse( &s ) );
			linewidth = atoi( token );
			w = (1.0f / p->columns) * (linewidth + 2);

			strcpy( token, COM_Parse( &s ) );
			lines = atoi( token );
			h = (1.0f / p->rows) * lines;
			JUSTIFY;
			DrawBox3D( x,y,linewidth,lines,p,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "boxtext" )) // specify a box to draw with text inside of it
		{
			int width,height;

			strcpy( token, COM_Parse( &s ) );
			GetBoxTextDimensions( token,&width,&height );
			w = (1.0f / p->columns) * width;
			h = (1.0f / p->rows) * height;
			JUSTIFY;
			TSTRING;
			DrawBoxText3D( x,y,token,p,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "dialogstring" )) // dialog
		{
			int width,height;

			a = con_diatime_off / 1.0f;

			if (a > 1.0f)
				a = 1.0f;

			GetBoxTextDimensions( dialog_string,&width,&height );
			w = (1.0f / p->columns) * width;
			h = (1.0f / p->rows) * height;
			JUSTIFY;
			TSTRING;
			DrawBoxText3D( x,y,dialog_string,p,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "cvarstring" )) // specify a cvar to draw at the current coordinate
		{
			strcpy( token, COM_Parse( &s ) );
			JUSTIFY_STRING( token );
			JUSTIFY;
			CURSOR;
			TSTRING;
			DrawString3D( x,y,Cvar_VariableString( token ),p,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "string" )) // specify a string to draw at the current coordinate
		{
			strcpy( token, COM_Parse( &s ) );
			JUSTIFY_STRING( token );
			JUSTIFY;
			CURSOR;
			TSTRING;
			DrawString3D( x,y,token,p,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "statstring" )) // specify a configstring
		{
			int value;
			char  *string;

			strcpy( token, COM_Parse ( &s ));
			value = cl.frame.playerstate.stats[atoi( token )];

			if ( cl.configstrings[value][0])
			{
				string = cl.configstrings[value];
				JUSTIFY_STRING( string );
				JUSTIFY;
				CURSOR;
				TSTRING;
				DrawString3D( x,y,string,p,r,g,b,a );
				CLEAR_JUSTIFY;
			}
			continue;
		}
		if (!strcmp( token, "istring" )) // specify a configstring
		{
			int value;
			char  *string;

			strcpy( token, COM_Parse ( &s ));
			value = cl.frame.playerstate.stats[atoi( token )];

			if ( cl.configstrings[CS_ITEMS+value][0])
			{
				string = cl.configstrings[CS_ITEMS+value];
				JUSTIFY_STRING( string );
				JUSTIFY;
				CURSOR;
				TSTRING;
				DrawString3D( x,y,string,p,r,g,b,a );
				CLEAR_JUSTIFY;
			}
			continue;
		}
		if (!strcmp( token, "drawsave" )) // draw a savegame slot
		{
			int gamenum;

			MenuOnly( p );
			gamenum = atoi( COM_Parse( &s ));
			sprintf ( temp,"%s",m_savestrings[gamenum] );
			JUSTIFY_STRING( temp );
			JUSTIFY;
			CURSOR;
			DrawString3D( x,y,temp,p,r,g,b,a );
			CLEAR_JUSTIFY;
		}
		if (!strcmp( token, "drawnotify" )) // draw the notify text
		{
			int lines;
			MenuOnly( p );
			lines = atoi( COM_Parse( &s ) );
			CONSOLE_DrawNotify( x,y,lines,p,r,g,b,a );
		}
		if (!strcmp( token, "drawmessage" )) // draw the notify text
		{
			int linewidth;
			MenuOnly( p );
			strcpy( token,COM_Parse( &s ));
			linewidth = atoi( token );
			CONSOLE_DrawMessage( x,y,linewidth,p,r,g,b,a );
		}
		if (!strcmp( token, "drawserver" )) // Draw a server slot
		{
			int servernum;

			MenuOnly( p );
			servernum = atoi( COM_Parse( &s ));
			sprintf ( temp,"%s",local_server_names[servernum] );

			// Remove the newline
			for (i=0; i<strlen( temp ); i++)
				if (temp[i] == '\n')
					temp[i] = 0;

			JUSTIFY_STRING( temp );
			JUSTIFY;
			CURSOR;
			DrawString3D( x,y,temp,p,r,g,b,a );
			CLEAR_JUSTIFY;
		}
		if (!strcmp( token, "bindstring" )) // Draw a key binding from a command
		{
			const char     *keyname;
			char temp2[64];
			int keys[2],doublekeys[2];

			strcpy( token,COM_Parse( &s ));

			MenuOnly( p );
			MENU_FindKeysForCommand( token,keys );
			MENU_FindDoubleKeysForCommand( token,doublekeys );

			// Expand the keys to their strings
			if (keys[0] == -1)
				keyname = "???";
			else
				keyname = Key_KeynumToString ( keys[0] );

			sprintf( temp,"%s",keyname );

			if (keys[1] != -1)
			{
				keyname = Key_KeynumToString ( keys[1] );
				sprintf( temp2," or %s",keyname );
				strcat( temp, temp2 );
			}

			// Expand the double bind keys to their strings
			if (doublekeys[0] == -1)
				keyname = NULL;
			else
				keyname = Key_KeynumToString ( doublekeys[0] );

			// Append the double binds
			if ( keyname )
			{
				sprintf( temp2," or (%s)",keyname );
				strcat( temp, temp2 );

				if ( doublekeys[1] != -1 )
				{
					keyname = Key_KeynumToString ( doublekeys[1] );
					sprintf( temp2," or (%s)",keyname );
					strcat( temp, temp2 );
				}
			}

			JUSTIFY_STRING( temp );
			JUSTIFY;
			CURSOR;
			DrawString3D( x,y,temp,p,r,g,b,a );
			CLEAR_JUSTIFY;
		}
		if (!strcmp( token, "drawbind" )) // Draw a key binding
		{
			const char     *keyname;
			char temp2[64];
			int keys[2],doublekeys[2];
			menubind3d_t   *bind;

			bind = &item->bind;

			MenuOnly( p );
			MENU_FindKeysForCommand( bind->command,keys );
			MENU_FindDoubleKeysForCommand( bind->command,doublekeys );

			// Expand the keys to their strings
			if (keys[0] == -1)
				keyname = "???";
			else
				keyname = Key_KeynumToString ( keys[0] );

			sprintf( temp,"%15s   %s", bind->name,keyname );

			if (bind->bind_grab_active)
			{
				temp[16] = '=';
			}

			if (keys[1] != -1)
			{
				keyname = Key_KeynumToString ( keys[1] );
				sprintf( temp2," or %s",keyname );
				strcat( temp, temp2 );
			}

			// Expand the double bind keys to their strings
			if (doublekeys[0] == -1)
				keyname = NULL;
			else
				keyname = Key_KeynumToString ( doublekeys[0] );

			// Append the double binds
			if ( keyname )
			{
				sprintf( temp2," or (%s)",keyname );
				strcat( temp, temp2 );

				if ( doublekeys[1] != -1 )
				{
					keyname = Key_KeynumToString ( doublekeys[1] );
					sprintf( temp2," or (%s)",keyname );
					strcat( temp, temp2 );
				}
			}

			JUSTIFY_STRING( temp );
			JUSTIFY;
			CURSOR;
			DrawString3D( x,y,temp,p,r,g,b,a );
			CLEAR_JUSTIFY;
		}
		if (!strcmp( token, "type" )) // specify strings to draw like a teletype
		{
			strcpy( token, COM_Parse( &s ) );
			c_teletype = atoi( token );

			if ( c_teletype == 1 )
				teletype_delay = 1.1;/* FIXME: teletype_delay is an 'int', not a 'float' */
			else if ( c_teletype == 2 )
				teletype_delay = 2.5;/* ditto... */

			continue;
		}
		if (!strcmp( token, "fill" ))
		{
			float red;
			float green;
			float blue;
			float alpha;

			red    = Sinatof( COM_Parse( &s ));
			green  = Sinatof( COM_Parse( &s ));
			blue   = Sinatof( COM_Parse( &s ));
			alpha  = Sinatof( COM_Parse( &s ));

			// Check for keywords to fill width, and height
			strcpy( token,COM_Parse( &s ));

			if (!Q_stricmp( token,"screenwidth" ))
				w = 1.0;
			else
				w = Sinatof( token ) / p->virtual_width;

			strcpy( token,COM_Parse( &s ));

			if (!Q_stricmp( token,"screenheight" ))
				h = 1.0;
			else
				h = Sinatof( token ) / p->virtual_height;

			alpha *= a;

			JUSTIFY;
			re.DrawFill3D( x,y,w,h,red,green,blue,alpha );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "fullfill" ))
		{
			float red    = Sinatof( COM_Parse( &s ));
			float green  = Sinatof( COM_Parse( &s ));
			float blue   = Sinatof( COM_Parse( &s ));
			float alpha  = Sinatof( COM_Parse( &s ));

			alpha *= a;

			w = 1.0f;
			h = 1.0f;

			JUSTIFY;
			re.DrawFill3D( 0,0,1,1,red,green,blue,alpha );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "bstring" )) // specify a string to draw at the current coordinate
		{
			strcpy( token, COM_Parse( &s ) );
			if ( (cls.realtime / 100) % p->blinkrate)
			{
				w = (1.0f / p->columns) * (strlen( token ));
				h = (1.0f / p->rows);
				JUSTIFY;
				JUSTIFY_STRING( token );
				CURSOR;
				TSTRING;
				DrawString3D( x,y,token,p,r,g,b,a );
				CLEAR_JUSTIFY;
			}
			continue;
		}
		if (!strcmp( token, "hstring" )) // specify a highlighted string to draw at the current coordinate
		{
			float red = Sinatof( COM_Parse( &s ));
			float green = Sinatof( COM_Parse( &s ));
			float blue = Sinatof( COM_Parse( &s ));

			strcpy( token, COM_Parse( &s ) );
			w = (1.0f / p->columns) * (strlen( token ));
			h = (1.0f / p->rows);
			JUSTIFY;
			JUSTIFY_STRING( token );
			CURSOR;
			TSTRING;
			re.DrawFill3D( x,y,w,h,red,green,blue,a );
			DrawString3D ( x,y,token,p,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "bhstring" )) // specify a blinking  highlighted string to draw at the current coordinate
		{
			float red = Sinatof( COM_Parse( &s ));
			float green = Sinatof( COM_Parse( &s ));
			float blue = Sinatof( COM_Parse( &s ));

			strcpy( token, COM_Parse( &s ) );
			if ( (cls.realtime / 100) % p->blinkrate)
			{
				w = (1.0f / p->columns) * (strlen( token ));
				h = (1.0f / p->rows);
				JUSTIFY;
				JUSTIFY_STRING( token );
				CURSOR;
				TSTRING;
				re.DrawFill3D( x,y,w,h,red,green,blue,a );
				DrawString3D ( x,y,token,p,r,g,b,a );
				CLEAR_JUSTIFY;
			}
			continue;
		}
		if (!strcmp( token, "boxstring" )) // Draw a string with a box around it
		{
			strcpy( token, COM_Parse( &s ) );
			w = (1.0f / p->columns) * ((strlen( token )+2));
			h = (1.0f / p->rows) * 2;
			JUSTIFY;
			JUSTIFY_STRING( token );
			CURSOR;
			TSTRING;
			DrawBoxString3D( x,y,token,p,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "string2" )) // specify a string to draw at the current coordinate
		{
			strcpy( token, COM_Parse( &s ) );
			w = (1.0f / p->columns) * (strlen( token ));
			h = (1.0f / p->rows);
			JUSTIFY;
			JUSTIFY_STRING( token );
			CURSOR;
			TSTRING;
			DrawAltString3D( x,y,token,p,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "bstring2" )) // specify a string to draw at the current coordinate
		{
			strcpy( token, COM_Parse( &s ) );
			if ( (cls.realtime / 100) % p->blinkrate)
			{
				w = (1.0f / p->columns) * (strlen( token ));
				h = (1.0f / p->rows);
				JUSTIFY;
				JUSTIFY_STRING( token );
				CURSOR;
				TSTRING;
				DrawAltString3D( x,y,token,p,r,g,b,a );
				CLEAR_JUSTIFY;
			}
			continue;
		}
		if (!strcmp( token, "hstring2" )) // specify a highlighted string to draw at the current coordinate
		{
			float red = Sinatof( COM_Parse( &s ));
			float green = Sinatof( COM_Parse( &s ));
			float blue = Sinatof( COM_Parse( &s ));

			strcpy( token, COM_Parse( &s ) );

			w = (1.0f / p->columns) * (strlen( token ));
			h = (1.0f / p->rows);
			JUSTIFY;
			JUSTIFY_STRING( token );
			CURSOR;
			TSTRING;
			re.DrawFill3D( x,y,w,h,red,green,blue,a );
			DrawAltString3D ( x,y,token,p,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "bhstring2" )) // specify a blinking  highlighted string to draw at the current coordinate
		{
			float red = Sinatof( COM_Parse( &s ));
			float green = Sinatof( COM_Parse( &s ));
			float blue = Sinatof( COM_Parse( &s ));

			strcpy( token, COM_Parse( &s ) );
			if ( (cls.realtime / 100) % p->blinkrate)
			{
				w = (1.0f / p->columns) * (strlen( token ));
				h = (1.0f / p->rows);
				JUSTIFY;
				JUSTIFY_STRING( token );
				CURSOR;
				TSTRING;
				re.DrawFill3D( x,y,w,h,red,green,blue,a );
				DrawAltString3D ( x,y,token,p,r,g,b,a );
				CLEAR_JUSTIFY;
			}
			continue;
		}
		if (!strcmp( token, "cb" )) // draw pic on the entire console
		{
			strcpy( token, COM_Parse( &s ) );
			re.DrawStretchPic3D( 0.0f,0.0f,1.0f,1.0f,token,r,g,b,a );
			continue;
		}
		if (!strcmp( token, "cbt" )) // draw tiled pic on the entire console background
		{
			strcpy( token, COM_Parse( &s ) );
			re.DrawTilePic3D( 0.0f,0.0f,1.f,1.0f,token,r,g,b,a );
			continue;
		}
		if (!strcmp( token, "picn" )) // draw pic name
		{
			int width,height;
			strcpy( token, COM_Parse( &s ) );

			re.DrawGetPicSize( &width,&height,token );
			w = (width)  / p->virtual_width;
			h = (height) / p->virtual_height;
			JUSTIFY;
			CURSOR;
			re.DrawPic3D( x,y,token,p->oovw,p->oovh,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "pic" )) // draw pic from an index in a stat
		{
			int width,height,value;

			strcpy( token, COM_Parse ( &s ));
			value = cl.frame.playerstate.stats[atoi( token )];

			if (value >= MAX_IMAGES)
				Com_Error ( ERR_DROP, "pic >= MAX_IMAGES" );

			if (cl.configstrings[CS_IMAGES+value][0])
			{
				re.DrawGetPicSize( &width,&height,cl.configstrings[CS_IMAGES+value] );
				w = (width)  / p->virtual_width;
				h = (height) / p->virtual_height;
				JUSTIFY;
				CURSOR;
				re.DrawPic3D( x,y,cl.configstrings[CS_IMAGES+value],p->oovw,p->oovh,r,g,b,a );
				CLEAR_JUSTIFY;
				continue;
			}
		}
		if (!strcmp( token, "dialogpic" )) // dialog pic
		{
			int width,height,value;

			value = dialog_icon;

			if (value >= MAX_IMAGES)
				Com_Error ( ERR_DROP, "dialogpic >= MAX_IMAGES" );

			a = con_diatime_off / 1.0f;

			if (a > 1.0f)
				a = 1.0f;

			if (cl.configstrings[CS_IMAGES+value][0])
			{
				re.DrawGetPicSize( &width,&height,cl.configstrings[CS_IMAGES+value] );
				w = (width)  / p->virtual_width;
				h = (height) / p->virtual_height;
				JUSTIFY;
				CURSOR;
				re.DrawPic3D( x,y,cl.configstrings[CS_IMAGES+value],p->oovw,p->oovh,r,g,b,a );
				CLEAR_JUSTIFY;
				continue;
			}
		}
		if (!strcmp( token, "icon" )) // draw an icon
		{
			int width,height,value;

			value = atoi( COM_Parse( &s ));

			if (value >= MAX_IMAGES)
				Com_Error ( ERR_DROP, "icon >= MAX_IMAGES" );

			if (cl.configstrings[CS_IMAGES+value][0])
			{
				re.DrawGetPicSize( &width,&height,cl.configstrings[CS_IMAGES+value] );
				w = (width)  / p->virtual_width;
				h = (height) / p->virtual_height;
				JUSTIFY;
				CURSOR;
				re.DrawPic3D( x,y,cl.configstrings[CS_IMAGES+value],p->oovw,p->oovh,r,g,b,a );
				CLEAR_JUSTIFY;
				continue;
			}
		}
		if (!strcmp( token, "drawinv" )) // draw floating inventory icons
		{
			int width,height,value;
			int num=0;
			// Loop through and count the number of icons
			for (i=0; i<MAX_ITEMS; i++)
			{
				if (cl.inventory[i].icon)
					num++;
			}

			for (i=0; i<num; i++)
			{
				value = cl.inventory[i].icon;

				if (value >= MAX_IMAGES)
					Com_Error ( ERR_DROP, "drawinv >= MAX_IMAGES" );

				if (cl.configstrings[CS_IMAGES+value][0])
				{
					char amount[32];
					re.DrawGetPicSize( &width,&height,cl.configstrings[CS_IMAGES+value] );
					w = (width)  / p->virtual_width;
					h = (height) / p->virtual_height;
					JUSTIFY;
					CURSOR;
					re.DrawPic3D( x,y,cl.configstrings[CS_IMAGES+value],p->oovw,p->oovh,r,g,b,a );
					sprintf( amount,"%d", cl.inventory[i].amount );
					DrawString3D( x,y,amount,p,r,g,b,a );
					CLEAR_JUSTIFY;
					x += width;
				}
			}
			continue;
		}
		if (!strcmp( token, "splinepos" )) // Sets the x and y position on the spine specified
		{
			float u;
			float *points;
			int num,numpoints;

			num = atoi( COM_Parse( &s ));
			numpoints = atoi( COM_Parse( &s ));

			// Make sure that the scripts aren't using reserved BSplines
			if (p == cls.mainconsole)
			{
				if (num < 32)
				{
					Com_Error( ERR_FATAL, "CONSOLE_DrawLayout3D: BSpline %d is reserved for the scripts\nPlease use BSplines 32-63\n",num );
				}
			}
			else if (num >= 32)
			{
				Com_Error( ERR_FATAL, "CONSOLE_DrawLayout3D: BSpline %d is reserved for the main console\nPlease use BSplines 0-31\n",num );
			}

			// Check to see if the spline exists, and create it if necessary
			if (!CL_BSplineExists( num ))
			{
				// Build up the points
				points = (float *)malloc(numpoints * 2 * sizeof(float));
				for (i=0; i<numpoints*2; i++)
					points[i] = Sinatof( COM_Parse( &s ));
				if (!CL_CreateBSpline( num,numpoints,points ))
				{
					Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "Could not create BSpline %d\n", num);
				}
				free( points );
			}
			// Get the time
			u = (cls.realtime - c_starttime) / 1000.0f;
			// Set the position
			CL_GetBSplinePosition( num,u,&x,&y );
		}
		if (!strcmp( token, "anim" )) // draw animation
		{
			int width,height;
			int f,num_frames;
			char animframe[MAX_OSPATH];

			strcpy( token, COM_Parse( &s ) );
			num_frames = atoi( token );
			f = (int)((cls.realtime / 100)%num_frames) + 1;
			strcpy( token, COM_Parse( &s ) );
			Com_sprintf( animframe, sizeof(animframe), "%s%d",token,f );

			re.DrawGetPicSize( &width,&height,animframe );
			w = width / p->virtual_width;
			h = height / p->virtual_height;
			JUSTIFY;
			CURSOR;
			re.DrawPic3D( x,y,animframe,p->oovw,p->oovh,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "animonce" )) // draw animation one time
		{
			int width,height;
			int f,num_frames;
			char animframe[MAX_OSPATH];

			strcpy( token, COM_Parse( &s ) );
			num_frames = atoi( token );

			f = (int)((cls.realtime - c_starttime)/100) + 1;
			if (f > num_frames)
				f = num_frames;

			strcpy( token, COM_Parse( &s ) );
			Com_sprintf( animframe, sizeof(animframe), "%s%d",token,f );

			re.DrawGetPicSize( &width,&height,animframe );
			w = width / p->virtual_width;
			h = height / p->virtual_height;
			JUSTIFY;
			CURSOR;
			re.DrawPic3D( x,y,animframe,p->oovw,p->oovh,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "spicn" )) // draw scaled pic name
		{
			float sx,sy;
			char pic[MAX_OSPATH];
			int width,height;

			strcpy( token, COM_Parse( &s ) );
			strncpy( pic,token,sizeof(pic)-1 );

			re.DrawGetPicSize( &width,&height,pic );

			sx = Sinatof( COM_Parse( &s ));
			sy = Sinatof( COM_Parse( &s ));

			w = sx * width / p->virtual_width;
			h = sy * height / p->virtual_height;

			sx /= p->virtual_width;
			sy /= p->virtual_height;

			JUSTIFY;
			CURSOR;
			re.DrawPic3D( x,y,pic,sx,sy,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "drawplmodel" )) // draw the player model
		{
			float width  = Sinatof( COM_Parse( &s ));
			float height = Sinatof( COM_Parse( &s ));
			JUSTIFY;
			MENU_DrawPlayerModel( x,y,width,height );
			CLEAR_JUSTIFY;
		}
		if (!strcmp( token, "drawinvmodel" )) // draw the player model
		{
			if ((p != cls.mainconsole) && (p != cls.genconsole))
			{
				Com_Error( ERR_FATAL, "CONSOLE_DrawLayout: drawinvmodel is not supported in game consoles\n" );
			}
			JUSTIFY;
			MENU_DrawInventoryModel( x,y,0,0,cl_inventory_scale );
			CLEAR_JUSTIFY;
		}
		if (!strcmp( token, "drawslider" )) // draw a slider object
		{
			menuslider3d_t *sl = &p->menu3d[p->current_menu_level]->item[itemnum]->slider;
			DrawSlider3D( x,y,sl,p,r,g,b,a );
			continue;
		}
		if (!strcmp( token, "drawfield" )) // draw a field object
		{
			float red,green,blue;
			const char *s2;
			menufield3d_t *f = &item->field;
			strcpy( token, COM_Parse( &s ) );
			s2 = token;
			red=r;green=g;blue=b;////
			while (s2)
			{
				const char *token2;
				token2 = COM_Parse( &s2 );
				if (!Q_stricmp( token2, "red" ))
				{
					red = Sinatof( COM_Parse( &s2 ));
					continue;
				}
				if (!Q_stricmp( token2, "green" ))
				{
					green = Sinatof( COM_Parse( &s2 ));
					continue;
				}
				if (!Q_stricmp( token2, "blue" ))
				{
					blue = Sinatof( COM_Parse( &s2 ));
					continue;
				}
			}
			w = (1.0f / (float)p->columns) * (f->visible_length - 1);
			h = (1.0f / (float)p->rows);
			JUSTIFY;
		////	DrawField3D( x,y,f,p,r,g,b,a,((itemnum == p->sel_menu_item) && (cls.currentconsole == p)));
			DrawField3D( x,y,f,p,red,green,blue,a,((itemnum == p->sel_menu_item) && (cls.currentconsole == p)));
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "drawlist" )) // draw a list object
		{
			const char *s2;
			char *buffer;
			int vis_chars=0;
			float red;
			float green;
			float blue;
			int curvalue;
			const char *itemname;

			red = 0;
			green = 0;
			blue = 0;

			curvalue = item->list.curvalue;

			if ( curvalue > item->list.numitems )
				curvalue = item->list.numitems - 1;

			if (item->list.itemnames)
				itemname = item->list.itemnames[curvalue];
			else
				itemname = "(Null)";

			strcpy( token, COM_Parse( &s ) );
			s2 = token;
			while (s2)
			{
				const char *token2;
				token2 = COM_Parse( &s2 );
				if (!Q_stricmp( token2, "numchars" ))
				{
					vis_chars = atoi( COM_Parse( &s2 )) + 1;
					continue;
				}
				if (!Q_stricmp( token2, "red" ))
				{
					red = Sinatof( COM_Parse( &s2 ));
					continue;
				}
				if (!Q_stricmp( token2, "green" ))
				{
					green = Sinatof( COM_Parse( &s2 ));
					continue;
				}
				if (!Q_stricmp( token2, "blue" ))
				{
					blue = Sinatof( COM_Parse( &s2 ));
					continue;
				}
			}

			if ( !vis_chars)
				continue;

			buffer = (char *)malloc(vis_chars);
			Com_sprintf( buffer,vis_chars,"%s",itemname );

			w = (1.0f / (float)p->columns) * (vis_chars - 1);
			h = (1.0f / (float)p->rows);
			JUSTIFY;
			re.DrawFill3D( x,y,w,h,red,green,blue,a );
			DrawString3D( x,y,buffer,p,r,g,b,a );
			CLEAR_JUSTIFY;
			free( buffer );
			continue;
		}
		if (!strcmp( token, "hnum" )) // health number
		{
			int color;
			int width = 3;
			int value = cl.frame.playerstate.stats[STAT_HEALTH];
			if (value > 25)
				color = 0;
			else if (value > 0)
				color = (cl.frame.serverframe>>2) & 1;      // flash
			else
				color = 1;

			JUSTIFY;
			CONSOLE_DrawStat( x,y,color,width,value,p );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "anum" )) // ammo number
		{
			int color;
			int width = 3;
			int value = cl.frame.playerstate.stats[STAT_AMMO];
			if (value > 5)
				color = 0;
			else if (value >= 0)
				color = (cl.frame.serverframe>>2) & 1;      // flash
			else
				continue;   // negative number = don't show

			JUSTIFY;
			CONSOLE_DrawStat( x,y,color,width,value,p );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "rnum" )) // armor number
		{
			int color;
			int width = 3;
			int value = cl.frame.playerstate.stats[STAT_ARMOR];

			if (value < 0)
				value = 0;

			color = 0;  // green
			JUSTIFY;
			CONSOLE_DrawStat( x,y,color,width,value,p );
			CLEAR_JUSTIFY;
			continue;
		}
		// 2015 code
		if (!strcmp( token, "lapnum" )) // display a lap time
		{
			float value = cl.frame.playerstate.stats[atoi( COM_Parse( &s ))];
			int lap_m;
			float lap_s;

			value *= 0.1;
			if (value < 0)
				value = 0;

			lap_m = floor( value/60 );
			lap_s = value - (float)lap_m*60.0;

			// limit lap times to 99:59.9
			if(lap_m > 99)
				lap_m = 99;
			if(lap_s > 59.9)
				lap_s = 59.9;

			sprintf( token, "%2i:%04.1f", lap_m, lap_s );
			JUSTIFY_STRING( token );
			JUSTIFY;
			CURSOR;
			TSTRING;
			DrawString3D( x,y,token,p,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}

		if (!strcmp( token, "fastestlap" )) // display the fastest lap time
		{
			sprintf( token, cl.configstrings[CS_CHECKPOINTS] );
			JUSTIFY_STRING( token );
			JUSTIFY;
			CURSOR;
			TSTRING;
			DrawString3D( x,y,token,p,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!Q_stricmp( token,"nonbikeitem" ))
		{
			// a player item is selected in the player setup menu
			display_player_on_bike = false;
			continue;
		}
		if (!Q_stricmp( token,"bikeitem" ))
		{
			// a bike item is selected in the player setup menu
			display_player_on_bike = true;
			continue;
		}
		if(!strcmp( token, "rangenum" )) // crosshair range number
		{
			float value;
			trace_t trace;
			vec3_t pos, end;
			extern int map_sequence_number;

			if ( (cls.state == ca_active) && (!cl_paused->value) && (cl.map_sequence_number == map_sequence_number) )
			{
				VectorCopy( cl.refdef.vieworg, pos );
				VectorMA( pos, 9999, cl.v_forward, end );
				trace = CL_GenericTrace( pos, vec3_origin, vec3_origin, end, MASK_SHOT, true, cl.playernum+1 );
				if(trace.fraction != 1)
				{
					// calc a range in meters at ~35 units per meter
					VectorSubtract( trace.endpos, pos, end );
					value = VectorLength( end ) * 0.0285;
				}
				else
				{
					// don't give a range if didn't hit anything
					value = 0;
				}
				sprintf( token,"%4.2fm", value );
				JUSTIFY_STRING( token );
				JUSTIFY;
				CURSOR;
				TSTRING;
				DrawString3D( x,y,token,p,r,g,b,a );
				CLEAR_JUSTIFY;
			}

			continue;
		}
		if (!strcmp( token, "num" )) // stat number
		{
			int width = 3;
			int value;
			value = cl.frame.playerstate.stats[atoi( COM_Parse( &s ))];

			w = width * CHAR_WIDTH / p->virtual_width;
			h = CHAR_HEIGHT / p->virtual_height;

			JUSTIFY;
			CONSOLE_DrawStat( x,y,0,width,value,p );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "smnum" )) // small stat number
		{
			int value;

			value = cl.frame.playerstate.stats[atoi( COM_Parse( &s ))];
			sprintf( token,"%d", value );
			JUSTIFY_STRING( token );
			JUSTIFY;
			CURSOR;
			TSTRING;
			DrawString3D( x,y,token,p,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "clipammo" )) // small stat number
		{
			int value;

			value = cl.frame.playerstate.stats[STAT_CLIPAMMO];
			if (value < 0)
				continue;
			sprintf( token,"%d", value );
			JUSTIFY_STRING( token );
			JUSTIFY;
			CURSOR;
			TSTRING;
			DrawString3D( x,y,token,p,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "drawsbar" )) // generic status bar
		{
			int min,max,snum,value;
			float red,green,blue,alpha;
			sbar_t *BarPtr;
			snum  = atoi( COM_Parse( &s ));
			BarPtr = &p->SBarPtr[snum];
			w     = BarPtr->w;
			h     = BarPtr->h;
			min   = BarPtr->min;
			max   = BarPtr->max;
			value = BarPtr->value;
			red   = BarPtr->red;
			green = BarPtr->green;
			blue  = BarPtr->blue;
			alpha = BarPtr->alpha;

			if (value <= min)
				continue;
			else if (value > max)
			{
				value = max;
			}

			w /= p->virtual_width;//height;
			h = (((float)value / max) * h) / p->virtual_height;

			JUSTIFY;
			re.DrawFill3D( x,y,w,h,red,green,blue,alpha );
			CLEAR_JUSTIFY;
		}
		if (!strcmp( token, "drawhsbar" )) // generic status bar
		{
			int min,max,snum,value;
			float red,green,blue,alpha;
			sbar_t *BarPtr;

			snum  = atoi( COM_Parse( &s ));
			BarPtr = &p->SBarPtr[snum];
			w     = BarPtr->w;
			h     = BarPtr->h;
			min   = BarPtr->min;
			max   = BarPtr->max;
			value = BarPtr->value;
			red   = BarPtr->red;
			green = BarPtr->green;
			blue  = BarPtr->blue;
			alpha = BarPtr->alpha;

			if (value <= min)
				continue;
			else if (value > max)
			{
				value = max;
			}

			h /= p->virtual_width;
			w = (((float)value / max) * w) / p->virtual_width;

			JUSTIFY;
			re.DrawFill3D( x,y,w,h,red,green,blue,alpha );
			CLEAR_JUSTIFY;
		}
		if (!strcmp( token, "arbar" )) // armor bar specific to a location
		{
			int value;
			float red,green,blue;

			value = cl.frame.playerstate.stats[atoi( COM_Parse( &s ))];

			if (value >= 100)
				value = 99;
			else if (value < 0)
				value = 0;

			w = Sinatof( COM_Parse( &s ));
			h = Sinatof( COM_Parse( &s ));
			w /= p->virtual_height;
			h /= p->virtual_height;

			red   = (float)(armor_lookup[(value<<2)  ]) / 255.0f;
			green = (float)(armor_lookup[(value<<2)+1]) / 255.0f;
			blue  = (float)(armor_lookup[(value<<2)+2]) / 255.0f;

			JUSTIFY;
			re.DrawAltFill3D( x,y,w,h,red,green,blue,0.3f );
			CLEAR_JUSTIFY;
		}
		if (!strcmp( token, "hbar" )) // health bar
		{
			int value = cl.frame.playerstate.stats[STAT_HEALTH];
			if (value > 100)
				value = 100;
			else if (value < 0)
				value = 0;

			w = Sinatof( COM_Parse( &s ));
			h = Sinatof( COM_Parse( &s ));

			w /= p->virtual_width;
			h /= p->virtual_height;
			JUSTIFY;
			re.DrawFill3D( x,y,w,h,0,1,0,1 );
			h = (1.0f - (float)value / 100.0f) * h;
			re.DrawFill3D( x,y,w,h,1,0,0,1 );
			CLEAR_JUSTIFY;
		}
		if (!strcmp( token, "rbar" )) // armor bar
		{
			int value = cl.frame.playerstate.stats[STAT_ARMOR];
			if (value > 100)
				value = 100;
			else if (value < 0)
				value = 0;

			w = Sinatof( COM_Parse( &s ));
			h = Sinatof( COM_Parse( &s ));

			w /= p->virtual_width;
			h /= p->virtual_height;
			JUSTIFY;
			re.DrawFill3D( x,y,w,h,0,1,0,1 );
			h = (1.0f - (float)value / 100.0f) * h;
			re.DrawFill3D( x,y,w,h,0,0,1,0.5 );
			CLEAR_JUSTIFY;
		}
		if (!strcmp( token, "abar" )) // ammo bar
		{
			int value = cl.frame.playerstate.stats[STAT_AMMO];
			float red,green,blue,alpha;

			red=0,green=1.0f,blue=0,alpha=1.0;
			w = Sinatof( COM_Parse( &s ));
			h = Sinatof( COM_Parse( &s ));

			if (value < 0)
				value = 0;
			else if (value < 15)
				red = 1.0f, green = 0, blue = 0;
			else if (value < 50)
				red = 1.0f, green = 1.0f, blue = 0;
			else if (value > 100)
			{
				alpha = 1.0f;
				value = 100;
			}
			w /= p->virtual_width;
			h = (((float)value / 100) * h) / p->virtual_height;

			JUSTIFY;
			re.DrawFill3D( x,y,w,h,red,green,blue,alpha );
			CLEAR_JUSTIFY;
		}
		if (!strcmp( token, "loadbar" )) // loading bar
		{
			float w2;

			w = Sinatof( COM_Parse( &s ));
			h = Sinatof( COM_Parse( &s ));
			MenuOnly( p );

			w /= p->virtual_width;
			h /= p->virtual_height;

			w2 = ( w * cl_loadbar_percentage );

			JUSTIFY;
			re.DrawFill3D( x,y,w,h,.6,.6,.6,1 );
			re.DrawFill3D( x,y,w2,h,0,0.1,0.3,1 );
			CLEAR_JUSTIFY;
		}
		if (!strcmp( token, "downloadbar" ) )
		{
			w = Sinatof( COM_Parse( &s ));
			h = Sinatof( COM_Parse( &s ));
			MenuOnly( p );

			w /= p->virtual_width;
			h /= p->virtual_height;

			w2 = ( w * ( cls.downloadpercent/100.0f ) );

			JUSTIFY;
			re.DrawFill3D( x,y,w,h,.6,.6,.6,1 );
			re.DrawFill3D( x,y,w2,h,0,0.1,0.3,1 );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "loadstring1" )) // loading string 1
		{
			MenuOnly( p );
			JUSTIFY_STRING( cl_status_string1 );
			JUSTIFY;
			CURSOR;
			TSTRING;
			DrawString3D( x,y,cl_status_string1,p,r,g,b,a );
			CLEAR_JUSTIFY;
		}
		if (!strcmp( token, "loadstring2" )) // loading string 2
		{
			MenuOnly( p );
			JUSTIFY_STRING( cl_status_string2 );
			JUSTIFY;
			CURSOR;
			TSTRING;
			DrawString3D( x,y,cl_status_string2,p,r,g,b,a );
			CLEAR_JUSTIFY;
		}
		if (!strcmp( token, "fabar" )) // Full 10 slot ammo bar
		{
			strcpy( token, COM_Parse( &s ) );
			CONSOLE_DrawFullAmmoBar( x,y,token,p );
		}
		if (!strcmp( token, "weaplist" )) // List of weapons
		{
			strcpy( token, COM_Parse( &s ) );
			CONSOLE_DrawWeaponList( x,y,token,p );
		}
		if (!strcmp( token, "time" )) // Current time
		{
			char tbuffer [10];

			_strtime2( tbuffer );
			w = (1.0f / p->columns) * (strlen( tbuffer ));
			h = (1.0f / p->rows);
			JUSTIFY;
			JUSTIFY_STRING( tbuffer );
			TSTRING;
			DrawString3D( x,y,tbuffer,p,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "date" )) // Current date
		{
			char dbuffer [10];

			_strdate2( dbuffer );
			w = (1.0f / p->columns) * (strlen( dbuffer ));
			h = (1.0f / p->rows);
			JUSTIFY;
			JUSTIFY_STRING( dbuffer );
			TSTRING;
			DrawString3D( x,y,dbuffer,p,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "fdate" )) // Future year date with current month/day
		{
			char dbuffer [10];

			i = 8;

			_strdate2( dbuffer );

			while (dbuffer[i] != '/')
				i--;

			strcpy( token, COM_Parse( &s ) );

			dbuffer[++i] = token[0];
			dbuffer[++i] = token[1];

			w = (1.0f / p->columns) * (strlen( dbuffer ));
			h = (1.0f / p->rows);
			JUSTIFY;
			JUSTIFY_STRING( dbuffer );
			TSTRING;
			DrawString3D( x,y,dbuffer,p,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "boxtime" )) // Current time in a box
		{
			char tbuffer [10];

			_strtime2( tbuffer );
			w = (1.0f / p->columns) * ((strlen( tbuffer )+2));
			h = 1.0f / p->rows * 3;
			JUSTIFY;
			JUSTIFY_STRING( tbuffer );
			TSTRING;
			DrawBoxString3D( x,y,tbuffer,p,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "boxdate" )) // Current date in a box
		{
			char dbuffer [9];
			_strdate2( dbuffer );
			w = (1.0f / p->columns) * ((strlen( dbuffer )+2));
			h = 1.0f / p->rows * 2;
			JUSTIFY;
			JUSTIFY_STRING( dbuffer );
			TSTRING;
			DrawBoxString3D( x,y,dbuffer,p,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "pickitem" ))
		{
			pickup_t *pick;
			int value;
			int width, height;
			float alpha;
			int count=0;

			MenuOnly( p );

			strcpy( token,COM_Parse( &s ));

			if (!Q_stricmp( token,"up" ))
				direction = UP;
			else if (!Q_stricmp( token,"down" ))
				direction = DOWN;
			else if (!Q_stricmp( token,"left" ))
				direction = LEFT;
			else if (!Q_stricmp( token,"right" ))
				direction = RIGHT;
			else {
				Com_Error ( ERR_DROP, "Pickarmor direction must be up,down,left,or,right\n" );
				direction = -1;/* not reached */
			}

			for (i=0, pick=cl_pickups; i< MAX_PICKUPS; i++, pick++)
			{
				if (!pick->iconindex || pick->endtime < cl.time)
					continue;

				value = pick->iconindex;

				if (!strncmp( "i_",cl.configstrings[CS_IMAGES+value],2 ))
				{
					alpha = ((float)(pick->endtime - cl.time) / (float)(PICKUP_FADE_TIME) );

					if (cl.configstrings[CS_IMAGES+value][0])
					{
						re.DrawGetPicSize( &width,&height,cl.configstrings[CS_IMAGES+value] );
						w = (width)  / p->virtual_width;
						h = (height) / p->virtual_height;
						JUSTIFY;
						re.DrawPic3D( x,y,cl.configstrings[CS_IMAGES+value],p->oovw,p->oovh,r,g,b,alpha );
						DrawString3D( x,y,conva( "%d", pick->amount ),p,r,g,b,alpha );
						CLEAR_JUSTIFY;
						count++;
						switch (direction)
						{
						case UP:
							y+=height;
							break;
						case DOWN:
							y-=height;
							break;
						case LEFT:
							x-=width;
							break;
						case RIGHT:
							x+=width;
							break;
						default:
							break;
						}
					}
				}
			}
		}
		if (!strcmp( token, "pickarmor" ))
		{
			pickup_t *pick;
			int value;
			int width, height;
			float alpha;
			int count=0;

			MenuOnly( p );

			strcpy( token,COM_Parse( &s ));

			if (!Q_stricmp( token,"up" ))
				direction = UP;
			else if (!Q_stricmp( token,"down" ))
				direction = DOWN;
			else if (!Q_stricmp( token,"left" ))
				direction = LEFT;
			else if (!Q_stricmp( token,"right" ))
				direction = RIGHT;
			else {
				Com_Error ( ERR_DROP, "Pickarmor direction must be up,down,left,or,right\n" );
				direction = -1;/* not reached */
			}

			for (i=0, pick=cl_pickups; i< MAX_PICKUPS; i++, pick++)
			{
				if (!pick->iconindex || pick->endtime < cl.time)
					continue;

				value = pick->iconindex;

				if (!strncmp( "a_",cl.configstrings[CS_IMAGES+value],2 ))
				{
					alpha = ((float)(pick->endtime - cl.time) / (float)(PICKUP_FADE_TIME) );

					if (cl.configstrings[CS_IMAGES+value][0])
					{
						re.DrawGetPicSize( &width,&height,cl.configstrings[CS_IMAGES+value] );
						w = (width)  / p->virtual_width;
						h = (height) / p->virtual_height;
						JUSTIFY;
						re.DrawPic3D( x,y,cl.configstrings[CS_IMAGES+value],p->oovw,p->oovh,r,g,b,alpha );
						DrawString3D( x,y,conva( "%d", pick->amount ),p,r,g,b,alpha );
						CLEAR_JUSTIFY;
						count++;
						switch (direction)
						{
						case UP:
							y+=height;
							break;
						case DOWN:
							y-=height;
							break;
						case LEFT:
							x-=width;
							break;
						case RIGHT:
							x+=width;
							break;
						default:
							break;
						}
					}
				}
			}
		}
		if (!strcmp( token, "pickweap" ))
		{
			pickup_t *pick;
			int value;
			int width, height;
			float alpha;
			int count=0;

			MenuOnly( p );

			strcpy( token,COM_Parse( &s ));

			if (!Q_stricmp( token,"up" ))
				direction = UP;
			else if (!Q_stricmp( token,"down" ))
				direction = DOWN;
			else if (!Q_stricmp( token,"left" ))
				direction = LEFT;
			else if (!Q_stricmp( token,"right" ))
				direction = RIGHT;
			else {
				Com_Error ( ERR_DROP, "Pickarmor direction must be up,down,left,or,right\n" );
				direction = -1;/* not reached */
			}

			for (i=0, pick=cl_pickups; i< MAX_PICKUPS; i++, pick++)
			{
				if (!pick->iconindex || pick->endtime < cl.time)
					continue;

				value = pick->iconindex;

				if (!strncmp( "w_",cl.configstrings[CS_IMAGES+value],2 ))
				{
					alpha = ((float)(pick->endtime - cl.time) / (float)(PICKUP_FADE_TIME) );

					if (cl.configstrings[CS_IMAGES+value][0])
					{
						re.DrawGetPicSize( &width,&height,cl.configstrings[CS_IMAGES+value] );
						w = (width)  / p->virtual_width;
						h = (height) / p->virtual_height;
						JUSTIFY;
						re.DrawPic3D( x,y,cl.configstrings[CS_IMAGES+value],p->oovw,p->oovh,r,g,b,alpha );
						DrawString3D( x,y,conva( "%d", pick->amount ),p,r,g,b,alpha );
						CLEAR_JUSTIFY;
						count++;
						switch (direction)
						{
						case UP:
							y+=height;
							break;
						case DOWN:
							y-=height;
							break;
						case LEFT:
							x-=width;
							break;
						case RIGHT:
							x+=width;
							break;
						default:
							break;
						}
					}
				}
			}
		}
		if (!strcmp( token, "pickhealth" ))
		{
			pickup_t *pick;
			int value;
			int width, height;
			float alpha;
			int count=0;

			MenuOnly( p );

			strcpy( token,COM_Parse( &s ));

			if (!Q_stricmp( token,"up" ))
				direction = UP;
			else if (!Q_stricmp( token,"down" ))
				direction = DOWN;
			else if (!Q_stricmp( token,"left" ))
				direction = LEFT;
			else if (!Q_stricmp( token,"right" ))
				direction = RIGHT;
			else {
				Com_Error ( ERR_DROP, "Pickarmor direction must be up,down,left,or,right\n" );
				direction = -1;/* not reached */
			}

			for (i=0, pick=cl_pickups; i< MAX_PICKUPS; i++, pick++)
			{
				if (!pick->iconindex || pick->endtime < cl.time)
					continue;

				value = pick->iconindex;

				if (!strncmp( "p_",cl.configstrings[CS_IMAGES+value],2 ))
				{
					alpha = ((float)(pick->endtime - cl.time) / (float)(PICKUP_FADE_TIME) );

					if (cl.configstrings[CS_IMAGES+value][0])
					{
						re.DrawGetPicSize( &width,&height,cl.configstrings[CS_IMAGES+value] );
						w = (width)  / p->virtual_width;
						h = (height) / p->virtual_height;
						JUSTIFY;
						re.DrawPic3D( x,y,cl.configstrings[CS_IMAGES+value],p->oovw,p->oovh,r,g,b,alpha );
						DrawString3D( x,y,conva( "%d", pick->amount ),p,r,g,b,alpha );
						CLEAR_JUSTIFY;
						count++;
						switch (direction)
						{
						case UP:
							y+=height;
							break;
						case DOWN:
							y-=height;
							break;
						case LEFT:
							x-=width;
							break;
						case RIGHT:
							x+=width;
							break;
						default:
							break;
						}
					}
				}
			}
		}
		if (!strcmp( token, "clientstatus" ) )
		{
			int value, clientnum, team, status;
			clientinfo_t *ci;
			const char *teamname="";
			const char *statusname="";

			strcpy( token, COM_Parse( &s ) );
			value = atoi( token );

			clientnum = cl.frame.playerstate.stats[value] & 0xff;

			team      = ( cl.frame.playerstate.stats[value] >> 8 )  & 0x03;
			status    = ( cl.frame.playerstate.stats[value] >> 10 ) & 0xff;
			clientnum -= 1;
			if (clientnum >= MAX_CLIENTS || clientnum < 0)
				Com_Error ( ERR_DROP, "Clientstatus message: client >= MAX_CLIENTS" );

			ci = &cl.clientinfo[clientnum];

			if ( team == 1 )
				teamname = "[ Hardcorps ]";
			else
				teamname = "[ Sintek ]";

			if ( status == 0 )
				statusname = "ENEMY";
			else if ( status == 1 )
				statusname = "Excellent";
			else if ( status == 2 )
				statusname = "Good";
			else if ( status == 3 )
				statusname = "Fair";
			else if ( status == 4 )
				statusname = "Poor";
			else if ( status == 5 )
				statusname = "Bad";
			else if ( status == 6 )
				statusname = "Dead";

			Com_sprintf( temp, sizeof(temp), "%s %s %s", ci->name, teamname, statusname );

			JUSTIFY_STRING( temp );
			JUSTIFY;
			CURSOR;
			TSTRING;
			DrawAltString3D( x,y,temp,p,r,g,b,a );
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "ctfci" ))
		{
			// draw a ctf client block
			int value,score, ping, tag=0, team=0;
			clientinfo_t   *ci;

			// Client to draw
			strcpy( token, COM_Parse( &s ) );
			value = atoi( token );

			if (value >= MAX_CLIENTS || value < 0)
				Com_Error ( ERR_DROP, "Deatmatch scoreboard message: client >= MAX_CLIENTS" );

			ci = &cl.clientinfo[value];

			score = atoi( COM_Parse( &s ));
			ping = atoi( COM_Parse( &s ));
			tag  = atoi( COM_Parse( &s ));
			team = atoi( COM_Parse( &s ));

			Com_sprintf( temp, sizeof(temp), "%3d %3d %-14.14s", score, ping, ci->name );
			JUSTIFY_STRING( temp );
			JUSTIFY;
			CURSOR;
			TSTRING;
			if ( tag )
				DrawAltString3D( x, y, temp, p, 1, 1, 1, 1 );
			else
			{
				if ( team == 1 )
				{
					DrawString3D( x, y, temp, p, .38, 0.84, 0.99, 1 );
				}
				else
				{
					DrawString3D( x, y, temp, p, 1.0, 0.48, 0.78, 1 );
				}
			}
			CLEAR_JUSTIFY;
			continue;
		}
		if (!strcmp( token, "client" ))
		{
			// draw a deathmatch client block
			int value,score, ping, time, tag;
			clientinfo_t   *ci;
			// This is centered on x
			p_centerx = true;

			// x value
			strcpy( token, COM_Parse( &s ) );
			x = Sinatof( token );

			// y value
			strcpy( token, COM_Parse( &s ) );
			y = Sinatof( token );

			// Client to draw
			strcpy( token, COM_Parse( &s ) );
			value = atoi( token );

			if (value >= MAX_CLIENTS || value < 0)
				Com_Error ( ERR_DROP, "Deatmatch scoreboard message: client >= MAX_CLIENTS" );

			ci = &cl.clientinfo[value];

			score = atoi( COM_Parse( &s ));
			ping = atoi( COM_Parse( &s ));
			time = atoi( COM_Parse( &s ));
			tag  = atoi( COM_Parse( &s ));

			x = viddef.width/2 - 160 + x;
			y = viddef.height/2 - 120 + y;

			if ( tag )
				re.DrawPic ( x+32, y, COM_Parse( &s ));

			DrawAltString ( x+32, y, ci->name );
			SINDrawString ( x+32, y+8,  "Score: " );
			DrawAltString ( x+32+7*8, y+8,  va( "%i", score ));
			SINDrawString ( x+32, y+16, va( "Ping:  %i", ping ));
			SINDrawString ( x+32, y+24, va( "Time:  %i", time ));

			if (!ci->icon)
				ci = &cl.baseclientinfo;

			re.DrawPic ( x, y, ci->iconname );
			continue;
		}
	}
}

#ifdef _MSC_VER
#pragma optimize("g", on)
#endif


/*
==========================================
CONSOLE_DrawConsole3D

Draws the scrolling console on the surface
==========================================
*/
static void CONSOLE_DrawConsole3D( console3d_t *p )
{
	int x,y,i;
	int rows;
	const char *text;
	int row;
	int lines;
	int end;
	float width,height;

	if (!p->con.initialized)
		CONSOLE_InitCon( p );

	lines  = p->virtual_height * p->fraction;
	width  = p->virtual_width / p->columns;
	height = p->virtual_height / p->rows;
	p->con.vislines = lines;

	// This calculates the number of rows that are drawn on the surface.
	rows = (lines / height) - 1;
	y = 1;

	// How many characters to draw.
	end = p->con.linewidth;
	row = p->con.display;
	for (i=0; i<rows; i++, y++, row--)
	{
		if (row < 0)
			break;
		text = p->con.text + (row % p->con.totallines)*p->con.linewidth;
		for (x=0; x<end; x++)
		{
			re.DrawChar3D((x*width)/p->virtual_width,(y*height)/p->virtual_height,text[x],p->rows,p->columns,p->r,p->g,p->b,p->a );
		}
	}
	re.FlushChar3D(p->r,p->g,p->b,p->a); /* FS */
}

/*
====================================
CONSOLE_DrawMenu3D

Draws the menu system on the surface
====================================
*/
static void CONSOLE_DrawMenu3D( console3d_t *p )
{
	int i;
	item3d_t *item;

	if ( !p->menu3d[p->current_menu_level] )
		return;

	item = p->menu3d[p->current_menu_level]->item[p->sel_menu_item];

	if ( item && (p->autotrigger_time > 0) && (cls.realtime > p->autotrigger_time ) )
	{
		p->autotrigger_time = 0;
		CONSOLE_ProcessMenuAction( p,item->action );
	}

	if (p->globlayout)
		DrawLayout3D( p->globlayout,p,0 );

	for (i=0; i<p->menu3d[p->current_menu_level]->num_menu_items; i++)
	{
		item3d_t *drawitem = p->menu3d[p->current_menu_level]->item[i];

		if (!drawitem)
			continue;

		if (!drawitem->menu_layout)
			continue;

		if (i == p->sel_menu_item)
			DrawLayout3D( drawitem->sel_menu_layout,p,i );
		else
			DrawLayout3D( drawitem->menu_layout,p,i );
	}
}

/*
================
CONSOLE_ConClear
================
*/
void CONSOLE_ConClear( console3d_t *p )
{
	p->con.x = 0;
	memset ( p->con.text, ' ', CON3D_TEXTSIZE );
}

/*
================
CONSOLE_Linefeed
================
*/
static void CONSOLE_Linefeed( console3d_t *p )
{
	p->con.x = 0;
	if (p->con.display == p->con.current)
		p->con.display++;
	p->con.current++;
	memset ( &p->con.text[(p->con.current%p->con.totallines)*p->con.linewidth], ' ', p->con.linewidth );
}

/*
================
CONSOLE_ConPrint
================
*/
void CONSOLE_ConPrint( console3d_t *p, const char *txt )
{
	int y;
	int c, l;
	static int cr;
	int mask;

	if (!p->con.initialized)
		CONSOLE_InitCon( p );

	if (txt[0] == 1 || txt[0] == 2)
	{
		mask = 128;     // go to colored text
		txt++;
	}
	else
		mask = 0;

	while ( (c = *txt) )
	{
		// count word length
		for (l=0; l< p->con.linewidth; l++)
			if ( txt[l] <= ' ')
				break;

		// word wrap
		if (l != p->con.linewidth && (p->con.x + l > p->con.linewidth) )
			p->con.x = 0;

		txt++;

		if (cr)
		{
			p->con.current--;
			cr = false;
		}

		if (!p->con.x)
		{
			CONSOLE_Linefeed ( p );
			// mark time for transparent overlay
			if (p->con.current >= 0)
				p->con.times[p->con.current % NUM_CON_TIMES] = cls.realtime;
		}

		switch (c)
		{
		case '\n':
			p->con.x = 0;
			break;

		case '\r':
			p->con.x = 0;
			cr = 1;
			break;
		default:    // display character and advance
			y = p->con.current % p->con.totallines;
			p->con.text[y*p->con.linewidth+p->con.x] = c | mask | p->con.ormask;
			p->con.x++;
			if (p->con.x >= p->con.linewidth)
				p->con.x = 0;
			break;
		}
	}
}

/*
===================
CONSOLE_FindConsole
===================
*/
static console3d_t *CONSOLE_FindConsole( const char *con_name )
{
	console3d_t *p;

	if (!strcmp( con_name,MAIN_CONSOLE ))
	{
		return cls.mainconsole;
	}
	else if (!strcmp( con_name,GENERIC_CONSOLE ))
	{
		return cls.genconsole;
	}

	for (p=active_consoles; p; p=p->next)
	{
		if (!strcmp( con_name, p->console_name ))
		{
			break;
		}
	}
	return p;
}

/*
===========================
CONSOLE_FindConsoleByNumber
===========================
*/
console3d_t *CONSOLE_FindConsoleByNumber( int number )
{
	console3d_t *p;
	for (p=active_consoles; p; p=p->next)
	{
		if (p->console_number == number)
		{
			break;
		}
	}
	return p;
}

/*
===========================
CONSOLE_SendCommandToScript
===========================
*/
static void CONSOLE_SendCommandToScript( console3d_t *p, const char *msg )
{
	char temp[1024];

	if (p == cls.mainconsole)
		return;

	sprintf( temp,"consolecmd %s ",p->console_name );

	Cbuf_AddText ( temp );
	Cbuf_AddText ( "\"" );
	Cbuf_AddText ( msg );
	Cbuf_AddText ( "\"" );
	Cbuf_AddText( "\n" );
}

/*
==========================
CONSOLE_InitializeVariable
==========================
*/
static void CONSOLE_InitializeVariable( const char *var_name, const char *msg, console3d_t *p )
{
	char temp[1024];

	if (p != cls.mainconsole)
	{
		// Send the variable to the script system on the server
		sprintf( temp,"consolevar %s ",var_name );
		Cbuf_AddText ( temp );
		Cbuf_AddText ( "\"" );
		Cbuf_AddText ( msg );
		Cbuf_AddText ( "\"" );
		Cbuf_AddText ( "\n" );
	}
}

/*
============================
CONSOLE_UpdateVariable
============================
*/
void CONSOLE_UpdateVariable( const char *var_name, const char *msg, console3d_t *p, float value, menu3dtype t )
{
	char temp[1024];
	cvar_t *cvar;
	extern cvar_t *Cvar_FindVar( const char *var_name );

	if (p == cls.mainconsole)
	{
		if (t == FIELD3D)
		{
			Cvar_Set( var_name,msg );
		}
		else
		{
			// Set the cvar to the current value of the menuitem.
			Cvar_SetValue ( var_name, value );
		}

		// Debugging information
		cvar = Cvar_FindVar ( var_name );
		if (cvar)
		{
			Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "Set cvar %s to %s\n", cvar->name,cvar->string);
		}
		return;
	}
	else
	{
		// Send the variable to the script system on the server
		sprintf( temp,"consolevar %s ",var_name );
		Cbuf_AddText ( temp );
		Cbuf_AddText ( "\"" );
		Cbuf_AddText ( msg );
		Cbuf_AddText ( "\"" );
		Cbuf_AddText ( "\n" );
	}
}

/*
========================
CONSOLE_ProcessInputLine
========================
*/
void CONSOLE_ProcessInputLine( console3d_t *p )
{
	CONSOLE_SendCommandToScript( p,p->KeyLinesPtr[p->edit_line]+1 );
}


/*
=============================
CONSOLE_SendCharacterToServer
=============================
*/
void CONSOLE_SendCharacterToServer( console3d_t *p, int key )
{
	char temp[1024];

	if (p == cls.mainconsole)
		return;

	if (key == K_ENTER)
	{
		sprintf( temp,"consoleret %d\n",p->console_number );
	}
	else if (key > 0)
	{
		sprintf( temp,"consolepos %d %d\n",p->console_number,key );
	}
	else
	{
		sprintf( temp,"consoleneg %d\n",p->console_number );
	}
	Cbuf_AddText( temp );
}

/*
============================
CONSOLE_SendMenuInfoToServer
============================
*/
static void CONSOLE_SendMenuInfoToServer( console3d_t *p )
{
	char temp[1024];

	if ((p == cls.mainconsole) || (p == cls.genconsole))
		return;

	// Send the menu level and the currently selected item to the server.
	sprintf( temp,"consolemenu %d %d %d\n", p->console_number, p->current_menu_level, p->sel_menu_item );
	Cbuf_AddText( temp );
}

/*
=============
CONSOLE_Clear
=============
*/
static void CONSOLE_Clear( const char *s )
{
	console3d_t *p;
	const char *con_name;

	con_name = COM_Parse( &s );
	p = CONSOLE_FindConsole( con_name );

	// Console not found
	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_ConClear: Tried to clear a non-existent console %s\n", con_name);
		return;
	}

	CONSOLE_ConClear( p );
}

/*
==============
CONSOLE_Printf
==============
*/
#if 0 /* not used */
static void CONSOLE_Printf( const char *con_name, const char *fmt, ... )
{

	va_list argptr;
	char msg[MAXPRINTMSG];
	console3d_t    *p;

	p = CONSOLE_FindConsole( con_name );

	if (!p)
	{
		Com_DPrintf( "CONSOLE_Printf: Tried to print to a non-existent console %s\n", con_name );
		return;
	}

	va_start ( argptr,fmt );
	Q_vsnprintf ( msg,sizeof(msg),fmt,argptr );
	va_end   ( argptr );
	msg[sizeof(msg)-1] = 0;

	CONSOLE_ConPrint( p,msg );
}
#endif


/*
==================
CONSOLE_ParsePickupList
==================
*/
static void CONSOLE_ParsePickupList( const char *s )
{
	pickup_t *pick;
	int i;

	// Parse the pickup list and add them to the cl_pickups

	// find a free pickup
	for (i=0, pick=cl_pickups; i< MAX_PICKUPS; i++, pick++)
	{
		if (!pick->iconindex || pick->endtime < cl.time)
		{
			pick->iconindex = atoi( COM_Parse( &s ));
			pick->amount    = atoi( COM_Parse( &s ));
			pick->endtime   = cl.time + PICKUP_FADE_TIME;
		}
	}
}

/*
==============
CONSOLE_Layout
==============
*/
static void CONSOLE_Layout( const char *s, qboolean append )
{
	console3d_t    *p;
	const char *con_name;

	con_name = COM_Parse( &s );
	p = CONSOLE_FindConsole( con_name );

	if (!p) {
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_Layout: Tried to layout a non-existent console %s\n", con_name);
		return;
	}

	if (append) {
		strcat( p->LayoutPtr, s );
	} else {
		strcpy( p->LayoutPtr, s );
	}
}

/*
==================
CONSOLE_LayoutFile
==================
*/
static void CONSOLE_LayoutFile( const char *s )
{
	console3d_t *p;
	const char   *con_name;
	const char  *path;
	void        *buffer;

	con_name = COM_Parse( &s );
	p = CONSOLE_FindConsole( con_name );

	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_LayoutFile: Tried to layout a non-existent console %s\n", con_name);
		return;
	}

	path = COM_Parse( &s );
	FS_LoadFile ( path, &buffer );

	strcpy( p->LayoutPtr, (char *)buffer );

	FS_FreeFile( buffer );
}

/*
==========================
CONSOLE_LoadLayoutFromFile
==========================
*/
void CONSOLE_LoadLayoutFromFile( console3d_t *p, const char *path )
{
	void *buffer;

	FS_LoadFile ( path, &buffer );
	strcpy( p->LayoutPtr, (char *)buffer );
	FS_FreeFile( buffer );
}


extern menu_func_t functions[];
extern key_func_t key_functions[];

/*
======================
CONSOLE_SetKeyFunction
======================
*/
void CONSOLE_SetKeyFunction( console3d_t *p )
{
	int i;
	assert( p->menu3d );
	if (p->menu3d) {
		p->menu3d[p->current_menu_level]->keyfunc = NULL;
		i = 0;
		while (key_functions[i].string) {
			assert( p->menu3d[p->current_menu_level]->menu_name );
			// Hack to get all credit menus to work
			if ( strstr( p->menu3d[p->current_menu_level]->menu_name, "credit" ) ) {
				if ( strstr( p->menu3d[p->current_menu_level]->menu_name, key_functions[i].string ) ) {
					p->menu3d[p->current_menu_level]->keyfunc = key_functions[i].callback;
				}
			} else {
				if (!strcmp( p->menu3d[p->current_menu_level]->menu_name,key_functions[i].string )) {
					p->menu3d[p->current_menu_level]->keyfunc = key_functions[i].callback;
					break;
				}
			}
			i++;
		}
	}
}

/*
========================
CONSOLE_FunctionCallback
========================
*/
static void CONSOLE_FunctionCallback( console3d_t *p, const char *s, void *self )
{
	int i;
	i = 0;
	while (functions[i].string) {
		if (!Q_stricmp( functions[i].string,s )) {
			functions[i].callback( self );
			break;
		}
		i++;
	}
}

/*
================
CONSOLE_SetFocus
================
*/
static void CONSOLE_SetFocus( const char *s )
{
	console3d_t    *p;
	const char *con_name;
	const char *focus;

	con_name = COM_Parse( &s );
	p = CONSOLE_FindConsole( con_name );

	if (!p) {
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_SetFocus: Tried to set a non-existent console %s\n", con_name);
		return;
	}

	focus = COM_Parse( &s );

	if (!strcmp( focus,"menu" )) {
		p->focus = MENU3D;
		if (cls.currentconsole == p) {
			cls.key_dest = key_menu3d;
		}
	} else if (!strcmp( focus,"console" )) {
		p->focus = CONSOLE3D;
		if (cls.currentconsole == p) {
			cls.key_dest = key_console3d;
		}
	}
}

/*
======================
CONSOLE_SetFocusByType
======================
*/
void CONSOLE_SetFocusByType( console3d_t *p, consolefocus_t focus )
{
	if (focus == MENU3D) {
		p->focus = MENU3D;
		if (cls.currentconsole == p) {
			cls.key_dest = key_menu3d;
		}
	} else if (focus == CONSOLE3D) {
		p->focus = CONSOLE3D;
		if (cls.currentconsole == p) {
			cls.key_dest = key_console3d;
		}
	}
}

/*
=====================
CONSOLE_SetForeground
=====================
*/
static void CONSOLE_SetForeground( const char *s )
{
	console3d_t    *p;
	const char       *con_name;

	con_name = COM_Parse( &s );
	p = CONSOLE_FindConsole( con_name );

	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_SetFocus: Tried to set a non-existent console %s\n", con_name);
		return;
	}

	p->r = Sinatof( COM_Parse( &s ));
	p->g = Sinatof( COM_Parse( &s ));
	p->b = Sinatof( COM_Parse( &s ));
	p->a = Sinatof( COM_Parse( &s ));
}

/*
====================
CONSOLE_SetMenuState
====================
*/
static void CONSOLE_SetMenuState( const char *s, qboolean active )
{
	console3d_t    *p;
	const char       *con_name;

	con_name = COM_Parse( &s );
	p = CONSOLE_FindConsole( con_name );

	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_SetMenuState: Tried to set a non-existent console %s\n", con_name);
		return;
	}

	p->drawmenu = active;
}

/*
====================
CONSOLE_SetStatusBar
====================
*/
static void CONSOLE_SetStatusBar( const char *s )
{
	console3d_t    *p;
	const char       *con_name;
	int snum;
	sbar_t *BarPtr;

	con_name = COM_Parse( &s );
	p = CONSOLE_FindConsole( con_name );

	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_SetStatusbar: Tried to set a non-existent console %s\n", con_name);
		return;
	}

	snum  = atoi( COM_Parse( &s ));
	BarPtr = &p->SBarPtr[snum];
	BarPtr->w     = Sinatof( COM_Parse( &s ));
	BarPtr->h     = Sinatof( COM_Parse( &s ));
	BarPtr->min   = Sinatof( COM_Parse( &s ));
	BarPtr->max   = Sinatof( COM_Parse( &s ));
	BarPtr->value = atoi( COM_Parse( &s ));
	BarPtr->red   = Sinatof( COM_Parse( &s ));
	BarPtr->green = Sinatof( COM_Parse( &s ));
	BarPtr->blue  = Sinatof( COM_Parse( &s ));
	BarPtr->alpha = Sinatof( COM_Parse( &s ));
}

/*
=========================
CONSOLE_SetStatusBarValue
=========================
*/
static void CONSOLE_SetStatusBarValue( const char *s )
{
	console3d_t    *p;
	const char       *con_name;
	int snum;

	con_name = COM_Parse( &s );
	p = CONSOLE_FindConsole( con_name );

	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_SetStatusbar: Tried to set a non-existent console %s\n", con_name);
		return;
	}

	snum  = atoi( COM_Parse( &s ));
	p->SBarPtr[snum].value = atoi( COM_Parse( &s ));
}


/*
==================
CONSOLE_SelectItem
==================
*/
void CONSOLE_SelectItem( console3d_t *p, int dir )
{
	int loop = 0;

	if (!p->menu3d[p->current_menu_level] || !p->menu3d[p->current_menu_level]->num_menu_items)
		return;

	p->sel_menu_item += dir;

	if (p->sel_menu_item == -1)
		p->sel_menu_item = p->menu3d[p->current_menu_level]->num_menu_items - 1;
	else
		p->sel_menu_item = (p->sel_menu_item) % p->menu3d[p->current_menu_level]->num_menu_items;

	// Skips over any items of type HEADER3D.
	while (p->menu3d[p->current_menu_level]->item[p->sel_menu_item]->type == HEADER3D)
	{
		if (p->sel_menu_item == 0)
		{
			loop++;
			if (loop > 3)
				break;
		}

		p->sel_menu_item += dir;

		if (p->sel_menu_item == -1)
		{
			p->sel_menu_item = p->menu3d[p->current_menu_level]->num_menu_items - 1;
		}
		else
		{
			p->sel_menu_item = (p->sel_menu_item) % p->menu3d[p->current_menu_level]->num_menu_items;
		}
	}

	// Send the input to the server
	CONSOLE_SendMenuInfoToServer( p );

	// Reset the typing timer
	CONSOLE_ResetTypeTimer( p );

	// Play a sound
	if (p->playsound)
		CL_RandomLocalSound( menu_move_sound );
}

/*
==================
CONSOLE_SkipHeader
==================
*/
void CONSOLE_SkipHeader( console3d_t *p )
{
	// Skips over any items of type HEADER3D.

	if (!p->menu3d)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "Console %s is not initialized\n", p->console_name);
		return;
	}

	while (p->menu3d[p->current_menu_level]->item[p->sel_menu_item]->type == HEADER3D)
	{
		p->sel_menu_item += 1;
		if (p->sel_menu_item >= p->menu3d[p->current_menu_level]->num_menu_items)
		{
			Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_SkipHeader: Menu %s does not have any non-header items\n", p->console_name);
			p->sel_menu_item = 0;
			return;
		}
	}
}

/*
=================
CONSOLE_SlideItem
=================
*/
void CONSOLE_SlideItem( console3d_t *p, int dir, item3d_t *item )
{
	menuslider3d_t *s;
	const char *st;
	const char *token;
	int prev_pos;

	switch (item->type)
	{
	case LIST3D:
		prev_pos = item->list.curvalue;
		item->list.curvalue += dir;
		if (item->list.curvalue < 0)
			item->list.curvalue = 0;
		else if (item->list.curvalue >= item->list.numitems)
			item->list.curvalue--;

		// Check for no change
		if (prev_pos == item->list.curvalue)
			break;

		// This is the general case for updating list items,  just use the
		// name of the list item and set it to the current value of the
		// integral index
		CONSOLE_UpdateVariable( item->list.name,
		                        item->list.itemnames[item->list.curvalue],
		                        p,
		                        item->list.curvalue,
		                        LIST3D );

		// Special case for updating list items. They can have actions
		// on each item in the list, or they can be "cvaritem"s where
		// they have a textstring stored in the actionitem which will be
		// used to set the cvar.
		if (item->list.itemactions)
		{
			// Check to see if this is a cvaritem list, if so
			// then all we need to do is set the cvar based off the actionitem
			// and we will not have to do a Cbuf_AddText
			if (item->localdata && strstr( item->localdata,"cvaritem" ))
			{
				if (item->list.itemactions[item->list.curvalue])
				{
					st = item->localdata;
					token = COM_Parse( &st );
					token = COM_Parse( &st );
					Cvar_Set ( token, item->list.itemactions[item->list.curvalue] );
					Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "Setting %s to %s\n", token, item->list.itemactions[item->list.curvalue]);
				}
			}
			else
			{
				if (item->list.itemactions[item->list.curvalue])
				{
					Cbuf_AddText( item->list.itemactions[item->list.curvalue] );
					Cbuf_Execute();
				}
			}
		}

		// Take care of any callbacks if there are any.
		if (item->action)
		{
			st = item->action;
			token = COM_Parse( &st );
			if (!strcmp( "callback",token ))
			{
				token = COM_Parse( &st );
				CONSOLE_FunctionCallback( p,token,item );
			}
		}
		if (p->playsound)
			CL_RandomLocalSound( menu_move_sound );
		break;
	case SLIDER3D:
	{
		char temp[64];
		float scale = 1.0;

		s = &item->slider;
		s->curvalue += (dir * (s->maxvalue - s->minvalue)/s->length);

		if ( s->curvalue > s->maxvalue )
			s->curvalue = s->maxvalue;
		else if ( s->curvalue < s->minvalue )
			s->curvalue = s->minvalue;

		// Check for a scale and divide by it before setting the cvar
		if (item->localdata)
		{
			const char *str = item->localdata;
			const char *token;
			while (1)
			{
				token = COM_Parse( &str );
				if (!token[0])
					break;

				if (!strcmp( token,"cvardivide" ))
				{
					scale = 1.0f / Sinatof( COM_Parse( &str ));
					continue;
				}
				if (!strcmp( token,"cvarscale" ))
				{
					scale = Sinatof( COM_Parse( &str ));
					continue;
				}
			}
		}

		sprintf(temp, "%f", s->curvalue*scale);
		CONSOLE_UpdateVariable( s->name,temp,p,s->curvalue*scale,SLIDER3D );

		// Take care of any callbacks if there are any.
		if (item->action)
		{
			st = item->action;
			token = COM_Parse( &st );
			if (!strcmp( "callback",token ))
			{
				token = COM_Parse( &st );
				CONSOLE_FunctionCallback( p,token,item );
			}
		}
		if (p->playsound)
			CL_RandomLocalSound( menu_move_sound );
		break;
	}
	default:
		break;
	}
}

/*
=====================
CONSOLE_ClearConsoles
=====================
*/
void CONSOLE_ClearConsoles( void )
{
	console3d_t *p;
	int i;

	for (p=active_consoles; p; p=p->next)
	{
		CONSOLE_Destroy( p );
		p->console_number = 0;
	}

	free_consoles = &cl_consoles[0];
	active_consoles = NULL;

	for (i=0; i<cl_numconsoles; i++)
		cl_consoles[i].next = &cl_consoles[i+1];

	cl_consoles[cl_numconsoles-1].next = NULL;
	console3d_init = true;

	for (i=0; i<MAX_PICKUPS; i++)
	{
		cl_pickups[i].iconindex = 0;
		cl_pickups[i].endtime = 0;
		cl_pickups[i].amount = 0;
	}
}

/*
===============
CONSOLE_PopMenu
===============
*/
qboolean CONSOLE_PopMenu( console3d_t *p )
{
	// Save the selected item in case we come back to this menu
	p->prev_sel_item[p->current_menu_level] = p->sel_menu_item;

	// If we are at the top level, then return
	if (p->current_menu_level == p->prev_menu_level[p->current_menu_level])
		return false;

	// Reset the timer for fades and animations
	CONSOLE_ResetTimer( p );
	CONSOLE_ResetTypeTimer( p );

	// Restore the level
	p->current_menu_level = p->prev_menu_level[p->current_menu_level];

	// Restore the selected item
	p->sel_menu_item = p->prev_sel_item[p->current_menu_level];

	// Set the key function
	CONSOLE_SetKeyFunction( p );

	// Send the input to the server
	CONSOLE_SendMenuInfoToServer( p );

	// Reset autotrigger time
	p->autotrigger_time = 0;

	// Run a menu action
	CONSOLE_ProcessMenuAction( p,p->menu3d[p->current_menu_level]->menuaction );

	// Play a sound
	if (p->playsound)
		CL_RandomLocalSound( menu_out_sound );
	return true;
}

/*
================
CONSOLE_PushMenu
================
*/
void CONSOLE_PushMenu( console3d_t *p, int level )
{
	// Save off the previous level and selected item to restore these
	// when we come back.
	int prev_level = p->current_menu_level;
	int prev_sel = p->sel_menu_item;

	CONSOLE_ResetTimer( p );
	CONSOLE_ResetTypeTimer( p );

	// Set the current level from the command.
	p->current_menu_level = level;

	// Reset autotrigger time
	p->autotrigger_time = 0;

	// Run a menu action
	CONSOLE_ProcessMenuAction( p,p->menu3d[p->current_menu_level]->menuaction );

	// Save the previous menu
	p->prev_menu_level[p->current_menu_level] = prev_level;

	// Save off the selection of the previous menu
	p->prev_sel_item[prev_level] = prev_sel;

	// Set the selection for this menu
	p->sel_menu_item = p->prev_sel_item[p->current_menu_level];

	// Set the key function
	CONSOLE_SetKeyFunction( p );

	// Skip over any headers
	CONSOLE_SkipHeader( p );

	// Send the input to the server
	CONSOLE_SendMenuInfoToServer( p );

	// Play a sound
	if (p->playsound)
		CL_RandomLocalSound( menu_in_sound );
}

/*
=========================
CONSOLE_ProcessMenuAction
=========================
*/
void CONSOLE_ProcessMenuAction( console3d_t *p, const char *s )
{
	const char *token;
	int i;

	p->autotrigger_time = 0;

	while (s)
	{
		token = COM_Parse( &s );
		// Change the menu level
		if (!strcmp( token,"pushmenu" ))
		{
			token = (COM_Parse( &s ));
			// Search for the menu name to get the level number
			for (i=0; i<p->numlevels; i++)
			{
				if (!p->menu3d[i]->menu_name)
					continue;
				if (!strcmp( token,p->menu3d[i]->menu_name ))
				{
					CONSOLE_PushMenu( p,i );
					break;
				}
			}
			continue;
		}
		if (!strcmp( token,"consolecmd" ))
		{
			CONSOLE_SendCommandToScript( p,s+1 );
			continue;
		}
		if (!strcmp( token,"popmenu" ))
		{
			qboolean ret;
			ret = CONSOLE_PopMenu( p );
			if (!ret)
			{
				// This is the top level
				cls.key_dest = key_game;
				cls.currentconsole = 0;
			}
		}
		if (!strcmp( token,"function" ))
		{
			token = (COM_Parse( &s ));
			CONSOLE_FunctionCallback( p,token,p->menu3d[p->current_menu_level]->item[p->sel_menu_item] );
			continue;
		}
		if (!strcmp( token,"stuffcmd" ))
		{
			token = (COM_Parse( &s ));
			Cbuf_AddText( token );
			continue;
		}
		if (!strcmp( token,"centerprint" ))
		{
			token = (COM_Parse( &s ));
			CONSOLE_CenterPrint( conva( "jcx jcy boxtext \"%s\"",token ));
			continue;
		}
		if (!strcmp( token, "autotrigger" ))
		{
			p->autotrigger_time = cls.realtime + Sinatof( COM_Parse( &s )) * 1000;
			continue;
		}
		if (!strcmp( token, "dminfo" ))
		{
			menuitem3d_t *menulevels[32];
			int j,index;
			qboolean menufound = false;

			// Load in the deathmatch menu layout file based on screen res
			if ( con_vidres == CON_HIRES )
			{
				if ( !CONSOLE_LoadMenu( va( "gencon maps/%s.mnu", p->menu3d[p->current_menu_level]->item[p->sel_menu_item]->list.itemnames[p->menu3d[p->current_menu_level]->item[p->sel_menu_item]->list.curvalue] ) ) )
				{
					continue;
				}
			}
			else
			{
				if ( !CONSOLE_LoadMenu( va( "gencon maps/%s_alt.mnu", p->menu3d[p->current_menu_level]->item[p->sel_menu_item]->list.itemnames[p->menu3d[p->current_menu_level]->item[p->sel_menu_item]->list.curvalue] ) ) )
				{
					continue;
				}
			}

			memset( menulevels, 0, sizeof(menulevels) );

			// Find the pointers in the generic menu
			for( i=0; i<cls.genconsole->numlevels; i++ )
			{
				if ( !cls.genconsole->menu3d[i]->menu_name )
					continue;

				if ( !strncmp( cls.genconsole->menu3d[i]->menu_name, "dm_mapinfo", 10 ) )
				{
					if ( strlen( cls.genconsole->menu3d[i]->menu_name ) > 10 )
					{
						index = cls.genconsole->menu3d[i]->menu_name[10] - '0';
						menulevels[index] = cls.genconsole->menu3d[i];
					}
				}
			}

			// Find the locations in the main menu and copy over the dminfo stuff
			for (i=0; i<cls.mainconsole->numlevels; i++)
			{
				if ( !cls.mainconsole->menu3d[i]->menu_name )
					continue;

				if ( !strncmp( cls.mainconsole->menu3d[i]->menu_name, "dm_mapinfo", 10 ) )
				{
					if ( strlen( cls.mainconsole->menu3d[i]->menu_name ) > 10 )
					{
						index = cls.mainconsole->menu3d[i]->menu_name[10] - '0';

						if ( !menulevels[index] )
							continue;

						if ( index == 1 )
							menufound = true;

						// Clear out the old menu items
						for (j=0; j<MAX_MENU3D_ITEMS; j++)
							cls.mainconsole->menu3d[i]->item[j] = 0;

						// Set the number of items
						cls.mainconsole->menu3d[i]->num_menu_items = menulevels[index]->num_menu_items;
						// Copy over the items to the main menu
						for (j=0; j<menulevels[index]->num_menu_items; j++)
						{
							cls.mainconsole->menu3d[i]->item[j] = menulevels[index]->item[j];
						}
					}
				}
			}
			if ( menufound )
				MENU_PushMenu( "dm_mapinfo1" );
			else
				Com_Printf( "Could not find dm_mapinfo1 menu level in maps/%s.mnu\n",
				            p->menu3d[p->current_menu_level]->item[p->sel_menu_item]->list.itemnames[p->menu3d[p->current_menu_level]->item[p->sel_menu_item]->list.curvalue] );
		}
	}

	// Send variable to the script
	if ( p->menu3d[p->current_menu_level]->item[p->sel_menu_item] &&
	     p->menu3d[p->current_menu_level]->item[p->sel_menu_item]->type == FIELD3D
	     )
	{
		CONSOLE_UpdateVariable( p->menu3d[p->current_menu_level]->item[p->sel_menu_item]->field.name,
		                        p->menu3d[p->current_menu_level]->item[p->sel_menu_item]->field.buffer,
		                        p,
		                        0,
		                        FIELD3D );
	}
}
/*
================
CONSOLE_LoadMenu
================
*/
qboolean CONSOLE_LoadMenu( const char *s )
{
	const char *con_name;
	const char *path;
	console3d_t *p;

	con_name = COM_Parse( &s );
	p = CONSOLE_FindConsole( con_name );

	// Console not found
	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_LoadMenu: Tried to load a menu for a non-existent console %s\n", con_name);
		return false;
	}

	path = SIN_Parse( &s );
	return ( CONSOLE_LoadMenuFromFile( p, path ) );
}

/*
=======================================
CONSOLE_DetermineHeightFromLayoutString
=======================================
*/
static float CONSOLE_DetermineHeightFromLayoutString( const char *s, console3d_t *p )
{
	const char *s1;
	char s2[MAX_STRING_CHARS];
	const char *token;
	int width;
	int height;
	float y;

	token = 0;

	y = p->virtual_height / p->rows;
	strcpy( s2, s );
	s1 = s2;
	while (1)
	{
		token = COM_GetToken( &s1, false );
		if (!token[0])
			break;
		if (!strncmp( token,"anim",4 ))
		{
			int i,numframes;
			token = COM_GetToken( &s1, false );
			numframes = atoi( token );
			token = COM_GetToken( &s1, false );

			for (i=1; i<=numframes; i++)
			{
				char temp[MAX_OSPATH];
				sprintf( temp,"%s%d", token, i );
				re.DrawGetPicSize( &width,&height,temp );
				y = __max( width, y );
			}
		}
		if (!Q_stricmp( token,"picn" ))
		{
			token = COM_GetToken( &s1, false );
			re.DrawGetPicSize( &width,&height,token );
			y = max( y,height );
		}
		if (!Q_stricmp( token,"box" ))
		{
			token = COM_GetToken( &s1, false );
			token = COM_GetToken( &s1, false );
			y = __max( y, (p->virtual_height / p->rows) * (atoi( token )+2));
		}
		if (strstr( token,"string" ))
		{
			token = COM_GetToken( &s1, false );
			GetBoxTextDimensions( token,&width,&height );
			y = __max( y, (p->virtual_height / p->rows) * height );
		}
		if (!Q_stricmp( token,"boxtext" ))
		{
			int width,height;
			token = COM_GetToken( &s1, false );
			GetBoxTextDimensions( token,&width,&height );
			y = __max( y,(p->virtual_height / p->rows) * (height+2));
		}
		if ( !(Q_stricmp( token,"drawfield" )) || !(strncmp( token,"box",3 )) )
		{
			y = __max( y,(p->virtual_height / p->rows) * 2 );
		}
	}
	return y;
}


static void ClearItemFromMainMenu( void *addr )
{
	console3d_t *p;
	int i,j;

	p = cls.mainconsole;

	for ( i=0; i<p->numlevels; i++ )
	{
		for ( j=0; j<MAX_MENU3D_ITEMS; j++ )
		{
			if ( p->menu3d[i]->item[j] == addr)
			{
				p->menu3d[i]->item[j] = 0;
			}
		}
	}
}

/*
=================
CONSOLE_ClearMenu
=================
*/
void CONSOLE_ClearMenu( console3d_t *p )
{

	int i,j;

	// HACK
	// If the main menu is pointing to any items that are about to
	// be freed, we need to zero out those pointers.

	if ( !strcmp( p->console_name, "gencon" ) )
	{
		for ( i=0; i<p->numlevels; i++ )
		{
			for ( j=0; j<MAX_MENU3D_ITEMS; j++ )
			{
				if ( p->menu3d[i]->item[j] )
					ClearItemFromMainMenu( p->menu3d[i]->item[j] );
			}
		}
	}

	Z_FreeTags( TAG_CONSOLE_BASE+p->console_number );

	if (p->globlayout) {
		p->globlayout[0] = 0;
	}
	return;
}

/*
=========================
CONSOLE_CountNumberLevels
=========================
*/
static int CONSOLE_CountNumberLevels( const char *s )
{
	const char *token;
	int count = 0;
	qboolean ifdemo = false;

	while ( 1 )
	{
		token = COM_GetToken( &s, true );
		if ( !s )
			return count;

		if (!strcmp( token,"ifdemo" ))
		{
			if ( !sindemo->value )
			{
				// This is not the demo version, so skip this code.
				token = COM_GetToken( &s, false );
				while ( s && strcmp( token,"endif" ) && strcmp( token,"else" ) )
				{
					token = COM_GetToken( &s, true );
				}
			}
			else
			{
				// We are now in an ifdemo statement, so parse to the else or endif.
				ifdemo = true;
			}
			continue;
		}
		if (!strcmp( token,"else" ))
		{
			if (ifdemo)
			{
				// We were in an if statement, so now skip the else part
				token = COM_GetToken( &s, false );
				while ( s && strcmp( token,"endif" ) )
				{
					token = COM_GetToken( &s, true );
				}
				ifdemo = false;
			}
			continue;
		}
		if ( !strcmp( token, "menulevel" ) )
		{
			count++;
			continue;
		}
	}
}


/*
========================
CONSOLE_LoadMenuFromFile
========================
*/
qboolean CONSOLE_LoadMenuFromFile( console3d_t *p, const char *path )
{
	const char   *token;
	char temptok[MAX_TOKEN_CHARS];
	void *buffer;
	int i,j,size;
	int level=-1;
	int item=0;
	float padding=-0;
	float y=0;
	qboolean autoheight=false;
	qboolean main_menu = (p == cls.mainconsole);
	char temp[1024];
	qboolean ifdemo = false;
	const char  *s;

	size = FS_LoadFile ( path, &buffer );
	if (size == -1)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_LoadMenuFromFile: Menu %s does not exist\n", path);
		return false;
	}

	CONSOLE_ClearMenu( p );

	s = (const char *) buffer;
	p->numlevels = CONSOLE_CountNumberLevels( s );

	if ( !p->numlevels )
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_LoadMenuFromFile: Menu %s does not have any menulevels\n", path);
		return false;
	}

	p->menu3d          = (menuitem3d_t **)Z_TagMalloc( p->numlevels * sizeof(menuitem3d_t *), TAG_CONSOLE_BASE + p->console_number );
	p->prev_menu_level = (int *)Z_TagMalloc( p->numlevels * sizeof(int), TAG_CONSOLE_BASE + p->console_number );
	p->prev_sel_item   = (int *)Z_TagMalloc( p->numlevels * sizeof(int), TAG_CONSOLE_BASE + p->console_number );

	while (1)
	{
		token = COM_GetToken( &s, true );
		if (!token[0])
			break;

		if (!strcmp( token,"nosound" ))
		{
			p->playsound = false;
			continue;
		}
		if (!strcmp( token,"ifdemo" ))
		{
			if ( !sindemo->value )
			{
				// This is not the demo version, so skip this code.
				token = COM_GetToken( &s, false );
				while ( s && strcmp( token,"endif" ) && strcmp( token,"else" ) )
				{
					token = COM_GetToken( &s, true );
				}
			}
			else
			{
				// We are now in an ifdemo statement, so parse to the else or endif.
				ifdemo = true;
			}
			continue;
		}
		if (!strcmp( token,"else" ))
		{
			if (ifdemo)
			{
				// We were in an if statement, so now skip the else part
				token = COM_GetToken( &s, false );
				while ( s && strcmp( token,"endif" ) )
				{
					token = COM_GetToken( &s, true );
				}
				ifdemo = false;
			}
			continue;
		}
		if (!strcmp( token,"globlayout" ))
		{
			token = COM_GetToken( &s, false );
			if (!p->globlayout)
				continue;
			else
				strcpy( p->globlayout, token );
		}
		if (!strcmp( token,"appgloblayout" ))
		{
			token = COM_GetToken( &s, false );
			if (!p->globlayout)
				continue;
			else
				strcat( p->globlayout," " );
			strcat( p->globlayout,token );
		}
		if (!Q_stricmp( token,"autoheight" ))
		{
			autoheight = true;
			token = COM_GetToken( &s, false );
			if (!Q_stricmp( token,"yt" )  || !Q_stricmp( token,"oyt" ))
			{
				token = COM_GetToken( &s, false );

				if (p->virtual_height >= 480)
					y = 480 + Sinatof( token );
				else
					y = 240 + Sinatof( token );
			}
			continue;
		}
		if (!Q_stricmp( token,"animcursor" ))
		{
			int width=0, height=0;
			token = COM_GetToken( &s, false );
			p->menu3d[level]->cursor.numframes = atoi( token );
			token = COM_GetToken( &s, false );
			p->menu3d[level]->cursor.name = CONSOLE_strdup( token,p );
			//strcpy(p->menu3d[level].cursor.name, token);

			for (i=1; i<=p->menu3d[level]->cursor.numframes; i++)
			{
				char temp[MAX_OSPATH];
				sprintf( temp,"%s%d", p->menu3d[level]->cursor.name, i );
				re.DrawGetPicSize( &width,&height,temp );
				p->menu3d[level]->cursor.width  = __max( width, p->menu3d[level]->cursor.width );
				p->menu3d[level]->cursor.height = __max( height, p->menu3d[level]->cursor.height );
			}
		}
		if (!Q_stricmp( token,"yspace" ))
		{
			token = COM_GetToken( &s, false );
			y -= Sinatof( token );
			continue;
		}
		if (!Q_stricmp( token,"padding" ))
		{
			token = COM_GetToken( &s, false );
			padding = Sinatof( token );
			continue;
		}
		if (!Q_stricmp( token,"menulevel" ))
		{
			level++;

			assert ( level < p->numlevels );

			token = COM_GetToken( &s, false );

			p->menu3d[level] = (menuitem3d_t *)Z_TagMalloc( sizeof(menuitem3d_t), TAG_CONSOLE_BASE + p->console_number );
			p->menu3d[level]->menu_name = CONSOLE_strdup( token,p );

			token = COM_GetToken( &s, false );
			if (token[0])
			{
				p->menu3d[level]->menuaction = CONSOLE_strdup( token,p );
			}

			// Reset the number for the new menu
			autoheight = false;
			item = 0;
			continue;
		}
		if (!Q_stricmp( token,"enditem" ))
		{
			item++;
			continue;
		}
		if (!Q_stricmp( token,"localdata" ))
		{
			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->localdata = CONSOLE_strdup( token,p );
			continue;
		}
		if (!Q_stricmp( token,"helpdata" ))
		{
			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->helpdata = CONSOLE_strdup( token,p );
			token = COM_GetToken( &s, false );
			if ( strlen( token ) )
				p->menu3d[level]->item[item]->helptime = Sinatof( token );
			else
				p->menu3d[level]->item[item]->helptime = 4;
			continue;
		}
		if (!Q_stricmp( token,"fade" ))
		{
			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->fade = Sinatof( token );
			token = COM_GetToken( &s, false );
			if ( strlen( token ) )
				p->menu3d[level]->item[item]->fade_delay = Sinatof( token );
			continue;
		}
		if (!Q_stricmp( token,"menuitem" ))
		{
			p->menu3d[level]->item[item] = ( item3d_t * )Z_TagMalloc( sizeof( item3d_t ), TAG_CONSOLE_BASE+p->console_number );

			p->menu3d[level]->item[item]->type = ACTION3D;
			token = COM_GetToken( &s, false );
			if (autoheight)
			{
				float height;
				strcpy( temptok,token );
				height = CONSOLE_DetermineHeightFromLayoutString( token,p );
				if (height < p->menu3d[level]->cursor.height)
					height += p->menu3d[level]->cursor.height / 2.0f;
				y -= height + padding;
				if (main_menu)
					sprintf( temp,"oyb %f %s",y,temptok );
				else
					sprintf( temp,"yb %f %s",y,temptok );
				p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup( temp,p );
			}
			else
			{
				p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup( token,p );
			}
			continue;
		}
		if (!Q_stricmp( token,"actionitem" ))
		{
			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->action = CONSOLE_strdup( token,p );
			continue;
		}
		if (!Q_stricmp( token,"binditem" ))
		{
			p->menu3d[level]->item[item] = ( item3d_t * )Z_TagMalloc( sizeof( item3d_t ), TAG_CONSOLE_BASE+p->console_number );

			p->menu3d[level]->item[item]->type = BIND3D;

			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->bind.command = CONSOLE_strdup( token,p );

			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->bind.name= CONSOLE_strdup( token,p );

			token = COM_GetToken( &s, false );
			if (autoheight)
			{
				float height;
				strcpy( temptok,token );
				height = CONSOLE_DetermineHeightFromLayoutString( token,p );
				if (height < p->menu3d[level]->cursor.height)
					height += p->menu3d[level]->cursor.height / 2.0f;
				y -= height + padding;
				if (main_menu)
					sprintf( temp,"oyb %f %s",y,temptok );
				else
					sprintf( temp,"yb %f %s",y,temptok );
				p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup( temp,p );
			}
			else
			{
				p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup( token,p );
			}
			continue;
		}
		if (!strncmp( token,"fielditem",9 ))
		{
			p->menu3d[level]->item[item] = ( item3d_t * )Z_TagMalloc( sizeof( item3d_t ), TAG_CONSOLE_BASE+p->console_number );

			if (strstr( token,"num" ))
				p->menu3d[level]->item[item]->field.flags |= M3DF_NUMBERSONLY;

			p->menu3d[level]->item[item]->type = FIELD3D;

			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->field.name = CONSOLE_strdup( token,p );

			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->field.length = atoi( token );

			p->menu3d[level]->item[item]->field.buffer = ( char * )Z_TagMalloc( p->menu3d[level]->item[item]->field.length * sizeof( char ), TAG_CONSOLE_BASE+p->console_number );

			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->field.visible_length = atoi( token );

			token = COM_GetToken( &s, false );
			strcpy( p->menu3d[level]->item[item]->field.buffer,token );

			token = COM_GetToken( &s, false );
			if (autoheight)
			{
				float height;
				strcpy( temptok,token );
				height = CONSOLE_DetermineHeightFromLayoutString( token,p );
				if (height < p->menu3d[level]->cursor.height)
					height += p->menu3d[level]->cursor.height / 2.0f;
				y -= height + padding;
				if (main_menu)
					sprintf( temp,"oyb %f %s",y,temptok );
				else
					sprintf( temp,"yb %f %s",y,temptok );
				p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup( temp,p );
			}
			else
			{
				p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup( token,p );
			}

			p->menu3d[level]->item[item]->field.cursor = 0;
			p->menu3d[level]->item[item]->field.visible_offset = 0;
			continue;
		}
		if (!Q_stricmp( token,"selitem" ))
		{
			token = COM_GetToken( &s, false );
			if (autoheight)
			{
				if (main_menu)
					sprintf( temp,"oyb %f %s",y,token );
				else
					sprintf( temp,"yb %f %s",y,token );
				p->menu3d[level]->item[item]->sel_menu_layout = CONSOLE_strdup( temp,p );
			}
			else
			{
				p->menu3d[level]->item[item]->sel_menu_layout = CONSOLE_strdup( token,p );
			}
			continue;
		}
		if (!Q_stricmp( token,"headeritem" ))
		{
			p->menu3d[level]->item[item] = ( item3d_t * )Z_TagMalloc( sizeof( item3d_t ), TAG_CONSOLE_BASE+p->console_number );

			p->menu3d[level]->item[item]->type = HEADER3D;
			token = COM_GetToken( &s, false );
			if (autoheight)
			{
				float height;
				strcpy( temptok,token );
				height = CONSOLE_DetermineHeightFromLayoutString( token,p );
				y -= height + padding;
				if (main_menu)
					sprintf( temp,"oyb %f %s",y,temptok );
				else
					sprintf( temp,"yb %f %s",y,temptok );
				p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup( temp,p );
				p->menu3d[level]->item[item]->sel_menu_layout = CONSOLE_strdup( temp,p );
			}
			else
			{
				p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup( token,p );
				p->menu3d[level]->item[item]->sel_menu_layout = CONSOLE_strdup( token,p );
			}
			continue;
		}
		if (!Q_stricmp( token,"deathmatchinfo_listitem" ))
		{
			p->menu3d[level]->item[item] = ( item3d_t * )Z_TagMalloc( sizeof( item3d_t ), TAG_CONSOLE_BASE+p->console_number );

			p->menu3d[level]->item[item]->type = LIST3D;

			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->list.name = CONSOLE_strdup( token,p );

			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->list.curvalue = atoi( token );

			token = COM_GetToken( &s, false );
			if (autoheight)
			{
				float height;
				strcpy( temptok,token );
				height = CONSOLE_DetermineHeightFromLayoutString( token,p );
				if (height < p->menu3d[level]->cursor.height)
					height += p->menu3d[level]->cursor.height / 2.0f;
				y -= height + padding;
				if (main_menu)
					sprintf( temp,"oyb %f %s",y,temptok );
				else
					sprintf( temp,"yb %f %s",y,temptok );
				p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup( temp,p );
			}
			else
			{
				p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup( token,p );
			}
			p->menu3d[level]->item[item]->list.numitems    = s_numdeathmatchmaps;
			p->menu3d[level]->item[item]->list.itemnames   = s_deathmatchmaps;
			p->menu3d[level]->item[item]->list.itemactions = 0;
		}
		if (!Q_stricmp( token,"playerskin_listitem" ))
		{
			p->menu3d[level]->item[item] = ( item3d_t * )Z_TagMalloc( sizeof( item3d_t ), TAG_CONSOLE_BASE+p->console_number );

			p->menu3d[level]->item[item]->type = LIST3D;

			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->list.name = CONSOLE_strdup( token,p );

			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->list.curvalue = atoi( token );

			token = COM_GetToken( &s, false );
			if (autoheight)
			{
				float height;
				strcpy( temptok,token );
				height = CONSOLE_DetermineHeightFromLayoutString( token,p );
				if (height < p->menu3d[level]->cursor.height)
					height += p->menu3d[level]->cursor.height / 2.0f;
				y -= height + padding;
				if (main_menu)
					sprintf( temp,"oyb %f %s",y,temptok );
				else
					sprintf( temp,"yb %f %s",y,temptok );
				p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup( temp,p );
			}
			else
			{
				p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup( token,p );
			}

			i = Cvar_VariableValue( "con_playermodel" );

			p->menu3d[level]->item[item]->list.numitems    = s_pmi[i].nskins;
			p->menu3d[level]->item[item]->list.itemnames   = s_pmi[i].SkinDisplayNamesPtr;
			p->menu3d[level]->item[item]->list.itemactions = 0;

			// Set the current value based on the model cvar and skin cvar
			for( i=0; i<p->menu3d[level]->item[item]->list.numitems; i++ )
			{
				if ( p->menu3d[level]->item[item]->list.itemnames[i] && skin && skin->string && !strcmp( p->menu3d[level]->item[item]->list.itemnames[i], skin->string ) )
				{
					p->menu3d[level]->item[item]->list.curvalue = i;
					break;
				}
			}
		}
		if (!Q_stricmp( token,"playermodel_listitem" ))
		{
			p->menu3d[level]->item[item] = ( item3d_t * )Z_TagMalloc( sizeof( item3d_t ), TAG_CONSOLE_BASE+p->console_number );

			p->menu3d[level]->item[item]->type = LIST3D;

			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->list.name = CONSOLE_strdup( token,p );

			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->list.curvalue = atoi( token );

			token = COM_GetToken( &s, false );
			if (autoheight)
			{
				float height;
				strcpy( temptok,token );
				height = CONSOLE_DetermineHeightFromLayoutString( token,p );
				if (height < p->menu3d[level]->cursor.height)
					height += p->menu3d[level]->cursor.height / 2.0f;
				y -= height + padding;
				if (main_menu)
					sprintf( temp,"oyb %f %s",y,temptok );
				else
					sprintf( temp,"yb %f %s",y,temptok );
				p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup( temp,p );
			}
			else
			{
				p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup( token,p );
			}

			p->menu3d[level]->item[item]->list.numitems    = s_numplayermodels;
			p->menu3d[level]->item[item]->list.itemnames   = s_pmnames;
			p->menu3d[level]->item[item]->list.itemactions = 0;

			Cvar_Set( "con_playermodel", "0" );

			// Set the current value based on the model cvar
			for( i=0; i<p->menu3d[level]->item[item]->list.numitems; i++ )
			{
				if ( p->menu3d[level]->item[item]->list.itemnames[i] && model && model->string && !strcmp( p->menu3d[level]->item[item]->list.itemnames[i], model->string ) )
				{
					p->menu3d[level]->item[item]->list.curvalue = i;
					Cvar_Set( "con_playermodel", va( "%i", i ) );
					break;
				}
			}
		}
		if (!Q_stricmp( token,"bikeskin_listitem" ))
		{
			p->menu3d[level]->item[item] = ( item3d_t * )Z_TagMalloc( sizeof( item3d_t ), TAG_CONSOLE_BASE+p->console_number );

			p->menu3d[level]->item[item]->type = LIST3D;

			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->list.name = CONSOLE_strdup( token,p );

			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->list.curvalue = atoi( token );

			token = COM_GetToken( &s, false );
			if (autoheight)
			{
				float height;
				strcpy( temptok,token );
				height = CONSOLE_DetermineHeightFromLayoutString( token,p );
				if (height < p->menu3d[level]->cursor.height)
					height += p->menu3d[level]->cursor.height / 2.0f;
				y -= height + padding;
				if (main_menu)
					sprintf( temp,"oyb %f %s",y,temptok );
				else
					sprintf( temp,"yb %f %s",y,temptok );
				p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup( temp,p );
			}
			else
			{
				p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup( token,p );
			}

			i = Cvar_VariableValue( "con_bikemodel" );

			p->menu3d[level]->item[item]->list.numitems    = s_bikemi[i].nskins;
			p->menu3d[level]->item[item]->list.itemnames   = s_bikemi[i].skindisplaynames;
			p->menu3d[level]->item[item]->list.itemactions = 0;

			// Set the current value based on the model cvar and skin cvar
			for( i=0; i<p->menu3d[level]->item[item]->list.numitems; i++ )
			{
				if ( p->menu3d[level]->item[item]->list.itemnames[i] && bikeskin && bikeskin->string && !strcmp( p->menu3d[level]->item[item]->list.itemnames[i], bikeskin->string ) )
				{
					p->menu3d[level]->item[item]->list.curvalue = i;
					break;
				}
			}
		}
		if (!Q_stricmp( token,"bikemodel_listitem" ))
		{
			p->menu3d[level]->item[item] = ( item3d_t * )Z_TagMalloc( sizeof( item3d_t ), TAG_CONSOLE_BASE+p->console_number );

			p->menu3d[level]->item[item]->type = LIST3D;

			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->list.name = CONSOLE_strdup( token,p );

			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->list.curvalue = atoi( token );

			token = COM_GetToken( &s, false );
			if (autoheight)
			{
				float height;
				strcpy( temptok,token );
				height = CONSOLE_DetermineHeightFromLayoutString( token,p );
				if (height < p->menu3d[level]->cursor.height)
					height += p->menu3d[level]->cursor.height / 2.0f;
				y -= height + padding;
				if (main_menu)
					sprintf( temp,"oyb %f %s",y,temptok );
				else
					sprintf( temp,"yb %f %s",y,temptok );
				p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup( temp,p );
			}
			else
			{
				p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup( token,p );
			}

			p->menu3d[level]->item[item]->list.numitems    = s_numbikemodels;
			p->menu3d[level]->item[item]->list.itemnames   = s_bikemnames;
			p->menu3d[level]->item[item]->list.itemactions = 0;

			Cvar_Set( "con_bikemodel", "0" );

			// Set the current value based on the model cvar
			for( i=0; i<p->menu3d[level]->item[item]->list.numitems; i++ )
			{
				if ( p->menu3d[level]->item[item]->list.itemnames[i] && bikemodel && bikemodel->string && !strcmp( p->menu3d[level]->item[item]->list.itemnames[i], bikemodel->string ) )
				{
					p->menu3d[level]->item[item]->list.curvalue = i;
					Cvar_Set( "con_bikemodel", va( "%i", i ) );
					break;
				}
			}
		}

		if (!Q_stricmp( token,"listitem" ))
		{
			// Start building a menulist
			// Get the layout first
			p->menu3d[level]->item[item] = ( item3d_t * )Z_TagMalloc( sizeof( item3d_t ), TAG_CONSOLE_BASE+p->console_number );

			p->menu3d[level]->item[item]->type = LIST3D;

			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->list.name = CONSOLE_strdup( token,p );

			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->list.curvalue = atoi( token );

			token = COM_GetToken( &s, false );
			if (autoheight)
			{
				float height;
				strcpy( temptok,token );
				height = CONSOLE_DetermineHeightFromLayoutString( token,p );
				if (height < p->menu3d[level]->cursor.height)
					height += p->menu3d[level]->cursor.height / 2.0f;
				y -= height + padding;
				if (main_menu)
					sprintf( temp,"oyb %f %s",y,temptok );
				else
					sprintf( temp,"yb %f %s",y,temptok );
				p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup( temp,p );
			}
			else
			{
				p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup( token,p );
			}

			// Make sure there are some list items
			token = COM_GetToken( &s, true );
			if (!Q_stricmp( token,"numitems" ))
			{
				int nitems = atoi( COM_GetToken( &s, false ));
				p->menu3d[level]->item[item]->list.numitems = nitems;
				p->menu3d[level]->item[item]->list.itemnames = (char **)Z_TagMalloc( sizeof(char *) * nitems,TAG_CONSOLE_BASE+p->console_number );
				p->menu3d[level]->item[item]->list.itemactions = (char **)Z_TagMalloc( sizeof(char *) * nitems,TAG_CONSOLE_BASE+p->console_number );
				for (i=0; i<nitems; i++)
				{
					token = COM_GetToken( &s, true );
					p->menu3d[level]->item[item]->list.itemnames[i] = CONSOLE_strdup( token,p );

					token = COM_GetToken( &s, false );
					if (token[0])
						p->menu3d[level]->item[item]->list.itemactions[i] = CONSOLE_strdup( token,p );

				}
				CONSOLE_InitializeVariable( p->menu3d[level]->item[item]->list.name,
				                            p->menu3d[level]->item[item]->list.itemnames[p->menu3d[level]->item[item]->list.curvalue],
				                            p );
			}
			else
			{
				Com_Error( ERR_FATAL, "CONSOLE_LoadMenuFromFile: numitems not specified for list item %s\n",p->menu3d[level]->item[item]->list.name );
			}
			continue;
		}
		if (!Q_stricmp( token,"slideritem" ))
		{
			p->menu3d[level]->item[item] = ( item3d_t * )Z_TagMalloc( sizeof( item3d_t ), TAG_CONSOLE_BASE+p->console_number );

			p->menu3d[level]->item[item]->type = SLIDER3D;
			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->slider.name = CONSOLE_strdup( token,p );
			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->slider.minvalue = Sinatof( token );
			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->slider.maxvalue = Sinatof( token );
			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->slider.curvalue = Sinatof( token );
			token = COM_GetToken( &s, false );
			p->menu3d[level]->item[item]->slider.length = atoi( token );
			token = COM_GetToken( &s, false );

			if (autoheight)
			{
				float height;
				strcpy( temptok,token );
				height = CONSOLE_DetermineHeightFromLayoutString( token,p );
				if (height < p->menu3d[level]->cursor.height)
					height += p->menu3d[level]->cursor.height / 2.0f;
				y -= height + padding;
				if (main_menu)
					sprintf( temp,"oyb %f %s ",y,temptok );
				else
					sprintf( temp,"yb %f %s ",y,temptok );
				p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup( temp,p );
			}
			else
			{
				p->menu3d[level]->item[item]->menu_layout = CONSOLE_strdup( token,p );
			}
			sprintf( temp,"%f",p->menu3d[level]->item[item]->slider.curvalue );

			CONSOLE_InitializeVariable( p->menu3d[level]->item[item]->slider.name,temp,p );
			continue;
		}
	}

	// Initialize the menus by counting up the items.
	for (i=0; i<p->numlevels; i++)
	{
		p->menu3d[i]->num_menu_items = 0;
		for (j=0; j<MAX_MENU3D_ITEMS; j++)
		{
			if (p->menu3d[i]->item[j] && p->menu3d[i]->item[j]->menu_layout)
				if (p->menu3d[i]->item[j]->menu_layout[0])
					p->menu3d[i]->num_menu_items++;
		}
	}
	p->current_menu_level = 0;
	p->sel_menu_item = 0;
	CONSOLE_SkipHeader( p );
	CONSOLE_SendMenuInfoToServer( p );

	FS_FreeFile( buffer );
	return true;
}


/*
===============
CONSOLE_Destroy
===============
*/
void CONSOLE_Destroy( console3d_t *p )
{
	int i;
	console3d_t *save;
//	assert(p->console_number != 0);

	save = p->next;

	free( p->globlayout );		/* Release the layout */
	free( p->LayoutPtr );
	i = 0;
	do {
		free( p->KeyLinesPtr[i] );
	} while (++i<32);
	free( p->con.text );
	free( p->SBarPtr );
	Z_FreeTags ( TAG_CONSOLE_BASE + p->console_number );
	memset( p,0,sizeof(console3d_t));
	p->next = save;
}

/*
==============
CONSOLE_Create
==============
*/
console3d_t *CONSOLE_Create( const char *s, qboolean scroll, qboolean menu )
{
	console3d_t *p;
	const char *con_name;
	int i;

	con_name = COM_Parse( &s );

	if (!free_consoles)
		Com_Error( ERR_FATAL, "CONSOLE_ParseInitString: No free consoles\n" );

	if (!strcmp( con_name,MAIN_CONSOLE )) {
		p = &mainconsole;
		if (!p->globlayout) {
			p->globlayout = (char *)calloc(1, 2000*sizeof(char));
		}
	} else if (!strcmp( con_name,GENERIC_CONSOLE )) {
		p = &genconsole;
		if (!p->globlayout) {
			p->globlayout = (char *)calloc(1, 2000*sizeof(char));
		}
	} else {
		p = free_consoles;
		free_consoles = p->next;
		p->next = active_consoles;
		active_consoles = p;
	}

	// p is the new console
	Com_sprintf ( p->console_name, sizeof(p->console_name), "%s", con_name );
	p->playsound         = true;
	p->active            = true;
	p->drawmenu          = true;
	p->console_number    = atoi( COM_Parse( &s ));
	p->virtual_width     = Sinatof( COM_Parse( &s ));
	p->oovw              = 1.0f / p->virtual_width;
	p->virtual_height    = Sinatof( COM_Parse( &s ));
	p->oovh              = 1.0f / p->virtual_height;
	p->rows              = atoi( COM_Parse( &s ));
	p->columns           = atoi( COM_Parse( &s ));
	p->fraction          = Sinatof( COM_Parse( &s ));
	p->scrollcon         = scroll;
	p->menucon           = menu;
	p->blinkrate         = 3;
	p->typerate          = 9;
	p->edit_line         = 0;
	p->history_line      = 0;
	p->r                 = 1.0;
	p->g                 = 1.0;
	p->b                 = 1.0;
	p->a                 = 1.0;
	if (p->LayoutPtr) {
		p->LayoutPtr[0] = 0;
	} else {
		p->LayoutPtr         = (char *)calloc(1, MAX_LAYOUT_LENGTH);
	}
	p->offsetx           = 0;
	p->offsety           = 0;
	if (!p->con.text) {
		p->con.text = (char *)malloc( CON3D_TEXTSIZE );
	}
	memset ( p->con.text,' ', CON3D_TEXTSIZE );
	p->prev_menu_level   = 0;
	p->prev_sel_item     = 0;
	p->menu3d            = 0;

	if (p->scrollcon && p->menucon) {
		p->focus = MENU3D;
	} else if (p->scrollcon) {
		p->focus = CONSOLE3D;
	} else if (p->menucon) {
		p->focus = MENU3D;
	} else {
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_Create: Console %s does not have menu or scroll set\n", p->console_name);
	}

	i = 0;
	do {
		if (!p->KeyLinesPtr[i]) {
			p->KeyLinesPtr[i] = (char *)calloc(1, MAXCMDLINE);
		}
		p->KeyLinesPtr[i][0] = ']';
		p->KeyLinesPtr[i][1] = 0;
	} while (++i<32);
	p->key_linepos = 1;

	p->current_menu_level = 0;
	return p;
//	Com_DPrintf("CONSOLE:Created %s\n",p->console_name);
}


void CONSOLE_SetType( console3d_t *p, int spawnflags )
{
	p->scrollcon         = (spawnflags & 2);
	p->menucon           = (spawnflags & 4);

	if ( p->focus != -1 )
		return;

	if (p->scrollcon && p->menucon)
	{
		p->focus = MENU3D;
	}
	else if (p->scrollcon)
	{
		p->focus = CONSOLE3D;
	}
	else if (p->menucon)
	{
		p->focus = MENU3D;
	}
	else
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_Create: Console %s does not have menu or scroll set\n", p->console_name);
	}
}

/*
==================
CONSOLE_CreateBase
==================
*/
void CONSOLE_CreateBase( int number )
{
	console3d_t *p;
	int i;

	if (!free_consoles)
		Com_Error( ERR_FATAL, "CONSOLE_ParseInitString: No free consoles\n" );

	p = free_consoles;
	free_consoles = p->next;
	p->next = active_consoles;
	active_consoles = p;

	// p is the new console
	p->console_name[0]   = 0;
	p->playsound         = true;
	p->active            = true;
	p->drawmenu          = true;
	p->console_number    = number;
	p->virtual_width     = 0;
	p->virtual_height    = 0;
	p->rows              = 0;
	p->columns           = 0;
	p->fraction          = 0;
	p->scrollcon         = false;
	p->menucon           = false;
	p->blinkrate         = 3;
	p->typerate          = 32;
	p->edit_line         = 0;
	p->old_edit_line     = 0;
	p->history_line      = 0;
	p->r                 = 1.0;
	p->g                 = 1.0;
	p->b                 = 1.0;
	p->a                 = 1.0;
	p->LayoutPtr         = (char *)calloc(1, MAX_LAYOUT_LENGTH * sizeof(char));
	p->type_index        = 1;
	p->focus             = (consolefocus_t)-1;
	p->con.text          = (char *)malloc( CON3D_TEXTSIZE * sizeof(char));
	memset ( p->con.text,' ', CON3D_TEXTSIZE );
	p->menu3d            = 0;
	p->prev_menu_level   = 0;
	p->prev_sel_item     = 0;

	p->SBarPtr = (sbar_t *)calloc(1, MAX_STATUSBARS * sizeof(sbar_t));
	for (i=0; i<32; i++)
	{
		p->KeyLinesPtr[i] = (char *)calloc(1, MAXCMDLINE * sizeof(char));
		p->KeyLinesPtr[i][0] = ']';
		p->KeyLinesPtr[i][1] = 0;
	}

	p->key_linepos = 1;
	p->old_key_linepos = 1;
	p->current_menu_level = 0;
}

/*
================
CONSOLE_Activate
================
*/
static void CONSOLE_Activate( const char *s )
{
	console3d_t *p;
	char con_name[MAX_CONSOLE_NAME];

	strcpy( con_name, COM_Parse( &s ));

	p = CONSOLE_FindConsole( con_name );

	if (p)
	{
		p->active = true;
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_Activate: Activated console %s\n", con_name);
	}
	else
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_Activate: Console %s not found, did you forget to create it?\n", con_name);
	}
}

/*
===========
CONSOLE_Use
===========
*/
static void CONSOLE_Use( const char *s )
{
	console3d_t *p;
	const char *name;

	name = COM_Parse( &s );
	p = CONSOLE_FindConsole( name );

	if ( !p )
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "Console %s does not exist\n", name);
		return;
	}

	// If we are already in the console, then get the hell out.
	if ((cls.key_dest == key_console3d) || (cls.key_dest == key_console3d))
	{
		cls.key_dest = key_game;
		cls.currentconsole = 0;
	}
	else
	{
		// Set up the console and the input focus
		cls.currentconsole = p;
		switch (p->focus)
		{
		case MENU3D:
			cls.key_dest = key_menu3d;
			break;
		case CONSOLE3D:
			cls.key_dest = key_console3d;
			break;
		default:
			if (p->scrollcon && p->menucon)
			{
				cls.key_dest = key_menu3d;
				p->focus = MENU3D;
			}
			else if (p->scrollcon)
			{
				cls.key_dest = key_console3d;
				p->focus = CONSOLE3D;
			}
			else
			{
				cls.key_dest = key_menu3d;
				p->focus = MENU3D;
			}
			break;
		}
	}
}

/*
=======================
CONSOLE_SetVirtualWidth
=======================
*/
static void CONSOLE_SetVirtualWidth( const char *s )
{
	console3d_t *p;
	const char *con_name;

	con_name = COM_Parse( &s );
	p = CONSOLE_FindConsole( con_name );

	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_SetVirtualWidth: Tried to set non-existent console:%s\n", con_name);
		return;
	}

	p->virtual_width = Sinatof( COM_Parse( &s ));
}

/*
========================
CONSOLE_SetVirtualHeight
========================
*/
static void CONSOLE_SetVirtualHeight( const char *s )
{
	console3d_t *p;
	const char *con_name;

	con_name = COM_Parse( &s );
	p = CONSOLE_FindConsole( con_name );

	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_SetVirtualHeight: Tried to set non-existent console:%s\n", con_name);
		return;
	}

	p->virtual_height = Sinatof( COM_Parse( &s ));
}

/*
===================
CONSOLE_SetFraction
===================
*/
static void CONSOLE_SetFraction( const char *s )
{
	console3d_t *p;
	const char *con_name;

	con_name = COM_Parse( &s );
	p = CONSOLE_FindConsole( con_name );

	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_SetFraction: Tried to set non-existent console:%s\n", con_name);
		return;
	}

	p->fraction = Sinatof( COM_Parse( &s ));
}

/*
==================
CONSOLE_SetColumns
==================
*/
static void CONSOLE_SetColumns( const char *s )
{
	console3d_t *p;
	const char *con_name;

	con_name = COM_Parse( &s );
	p = CONSOLE_FindConsole( con_name );

	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_SetColumns: Tried to set non-existent console:%s\n", con_name);
		return;
	}

	p->columns = atoi( COM_Parse( &s ));
	CONSOLE_ConCheckResize( p );
}

/*
===============
CONSOLE_SetRows
===============
*/
static void CONSOLE_SetRows( const char *s )
{
	console3d_t *p;
	const char *con_name;

	con_name = COM_Parse( &s );
	p = CONSOLE_FindConsole( con_name );

	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_SetRows: Tried to set non-existent console:%s\n", con_name);
		return;
	}

	p->rows = atoi( COM_Parse( &s ));
	CONSOLE_ConCheckResize( p );
}

/*
=============
CONSOLE_Print
=============
*/
static void CONSOLE_Print( const char *s )
{
	console3d_t *p;
	const char *con_name;
	const char *msg;

	con_name = COM_Parse( &s );
	p = CONSOLE_FindConsole( con_name );

	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_Print: Tried to print to non-existent console: %s\n", con_name);
		return;
	}

	msg = s+1;
	CONSOLE_ConPrint( p,msg );
}

/*
==================
CONSOLE_Deactivate
==================
*/
static void CONSOLE_Deactivate( const char *s )
{
	console3d_t *p;
	char con_name[MAX_CONSOLE_NAME];

	strcpy( con_name, COM_Parse( &s ));

	p = CONSOLE_FindConsole( con_name );

	if (p)
	{
		p->active = false;
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_Deactivate:Deactivated %s\n", con_name);
	}
	else
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_Deactivate: Console %s not found, did you forget to create it?\n", con_name);
	}
}

/*
=======================
CONSOLE_ScriptKickUsers
=======================
*/
static void CONSOLE_ScriptKickUsers( const char *s )
{
	console3d_t *p;
	const char *con_name;

	con_name = COM_Parse( &s );
	p = CONSOLE_FindConsole( con_name );

	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_KickUsers: Tried to kick users from a non-existent console:%s\n", con_name);
		return;
	}

	if (p == cls.currentconsole)
	{
		cls.key_dest = key_game;
		cls.currentconsole = 0;
	}

	Cbuf_AddText ( "exitcon" );
	Cbuf_AddText( "\n" );
}


/*
=======================
CONSOLE_PlayerKickUsers
=======================
*/
static void CONSOLE_PlayerKickUsers( const char *s )
{
	console3d_t *p;
	const char *con_name;

	con_name = COM_Parse( &s );
	p = CONSOLE_FindConsole( con_name );

	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_KickUsers: Tried to kick users from a non-existent console:%s\n", con_name);
		return;
	}

	if (p == cls.currentconsole)
	{
		cls.key_dest = key_game;
		cls.currentconsole = 0;
	}
}

/*
============
CONSOLE_Exit
============
*/
void CONSOLE_Exit( console3d_t *p )
{
	if (!p)
		return;

	if ((p == cls.mainconsole) || (p == cls.genconsole)) {
		//
		// restore anything we changed to go into the menu
		//
		cls.key_dest = key_game;
		cls.currentconsole = 0;
		Cvar_SetValue ( "paused", 0 );
	} else {
		Cbuf_AddText ( "exitcon\n" );
	}
}

/*
=====================
CONSOLE_FadeIn
=====================
*/
void CONSOLE_FadeIn( const char *s )
{
	con_fadetime_off     = Sinatof( COM_Parse( &s ));
	con_fadetime_start   = con_fadetime_off;
	con_fade_r           = Sinatof( COM_Parse( &s ));
	con_fade_g           = Sinatof( COM_Parse( &s ));
	con_fade_b           = Sinatof( COM_Parse( &s ));
	con_fade_a           = 0;
	con_fade_direction   = 1;
}

/*
=====================
CONSOLE_FadeOut
=====================
*/
static void CONSOLE_FadeOut( const char *s )
{
	con_fadetime_off     = Sinatof( COM_Parse( &s ));
	con_fadetime_start   = con_fadetime_off;
	con_fade_r           = Sinatof( COM_Parse( &s ));
	con_fade_g           = Sinatof( COM_Parse( &s ));
	con_fade_b           = Sinatof( COM_Parse( &s ));
	con_fade_a           = 0;
	con_fade_direction   = -1;
}


/*
=================
CONSOLE_PrintFile
=================
*/
static void *file_data = NULL;
static void CONSOLE_PrintFile( const char *s )
{
	char filename[MAX_OSPATH];

	if ( file_data )
	{
		FS_FreeFile( file_data );
	}

	COM_StripExtension( COM_Parse( &s ), filename );
	COM_DefaultExtension( filename, ".txt" );
	FS_LoadFile( va( "%s", filename ), &file_data );
	CONSOLE_ResetTypeTimer( cls.mainconsole );
}

static void CONSOLE_ClearPrintFile( const char *s )
{
	if ( file_data )
	{
		FS_FreeFile( file_data );
	}

	file_data = NULL;
}

/*
==========================
CONSOLE_LoadGenericOverlay
==========================
*/
static void *generic_overlay = NULL;
static void CONSOLE_LoadGenericOverlay( const char *s )
{
	char filename[MAX_OSPATH];

	if ( generic_overlay )
	{
		FS_FreeFile( generic_overlay );
	}

	strcpy( filename, COM_Parse( &s ) );

	if ( con_vidres == CON_HIRES )
		COM_DefaultExtension( filename, ".hdh" );
	else
		COM_DefaultExtension( filename, ".hdl" );

	FS_LoadFile( va( "layouts/%s", filename ), &generic_overlay );
}

/*
============================
CONSOLE_LoadIntermissionFile
============================
*/
static void CONSOLE_LoadIntermissionFile( const char *s )
{
	char filename[MAX_OSPATH];

	if ( intermission_data )
	{
		FS_FreeFile( intermission_data );
	}

	strcpy( filename, COM_Parse( &s ) );

	if ( con_vidres == CON_HIRES )
		COM_DefaultExtension( filename, ".imh" );
	else
		COM_DefaultExtension( filename, ".iml" );

	FS_LoadFile( va("%s", filename), &intermission_data );
}

/*
====================================
CONSOLE_SetupDefaultIntermissionFile
====================================
*/
void CONSOLE_SetupDefaultIntermissionLayoutFile( const char *s )
{
	strcpy( intermission_layoutfile, s );
}

/*
==============================
CONSOLE_LoadIntermissionLayout
==============================
*/
static void CONSOLE_LoadIntermissionLayout( const char *s )
{
	free( intermission_layout );
	intermission_layout = strdup( s );
}

/*
=============================
CONSOLE_ClearIntermissionData
=============================
*/
void CONSOLE_ClearIntermissionData_f( void )
{
	// Clear out intermission data
	if ( intermission_data ) {
		FS_FreeFile( intermission_data );
		intermission_data = NULL;
	}
	free( intermission_layout );
	intermission_layout = NULL;

	// Disable intermission screen
	cls.intermission_active = false;
	intermission_layoutfile[0] = 0;
	//
	// clear any dialog from last level
	//
	con_diatime_off = 0;

// 01-06-99 M. Steiger make sure pointer is valid....
//   if ( cl_paused->value )
	if ( cl_paused && cl_paused->value )
		CL_Pause_f();

}

/*
==================
CONSOLE_BeginLevel
==================
*/
void CONSOLE_BeginLevel_f( void )
{
	// Clear the background
	if ( cls.mainconsole )
	{
		DrawLayout3D( "jcx jcy picn entering", cls.mainconsole, -1 );
		re.EndFrame();
	}
	// Clear the status bars
	cl_status_current = 0;
}

/*
====================
CONSOLE_DeathMessage
====================
*/
static void CONSOLE_DeathMessage( const char *s )
{
	int attacker, victim, meansofdeath;
	const char     *message1=NULL, *message2="";
	clientinfo_t   *vi, *ai;
	int num;

	victim       = atoi( COM_Parse( &s ) );
	attacker     = atoi( COM_Parse( &s ) );
	meansofdeath = atoi( COM_Parse( &s ) );

	if ( victim >= MAX_CLIENTS || victim < 0 )
		Com_Error ( ERR_DROP, "Deatmatch death message: client >= MAX_CLIENTS" );

	vi = &cl.clientinfo[victim];

	if ( attacker == -1 )
	{
		// No attacker
		switch ( meansofdeath )
		{
		case MOD_FALLING:
			message1 = "fell down";
			break;
		case MOD_ADRENALINE:
			message1 = "overdosed on adrenaline";
			break;
		case MOD_ION_DESTRUCT:
			message1 = "overloaded his gun";
			break;
		default:
			message1  = "cracked under the pressure";
		}

		if ( message1 )
		{
			Com_Printf( "%s %s.\n", vi->name, message1 );
		}
		else
		{
			Com_Printf( "%s died.\n", vi->name );
		}
		return;
	}
	else
	{
		if ( attacker >= MAX_CLIENTS || victim < 0 )
			Com_Error( ERR_DROP, "Deatmatch death message: client >= MAX_CLIENTS" );

		ai = &cl.clientinfo[attacker];
		switch (meansofdeath)
		{
		case MOD_FISTS:   // FISTS
		{
			num = rand() % 6;
			switch (num)
			{
			case 0:
				message1 = "was knocked out by";
				break;
			case 1:
				message1 = "was pummelled by";
				break;
			case 2:
				message1 = "was over come by";
				message2 = "'s ninja technique";
				break;
			case 3:
				message1 = "ate a knuckle sandwich courtesy of";
				break;
			case 4:
				message1 = "takes";
				message2 = "'s beating";
				break;
			case 5:
				message1 = "liked";
				message2 = "'s death touch";
				break;
			default:
				message1 = "was killed by";
				message2 = "'s fists";
				break;
			}
		}
		break;
		case MOD_MUTANTHANDS:
			message1 = "was shredded by";
			break;
		case MOD_MAGNUM:   // Magnum
		{
			num = rand() % 5;
			switch (num)
			{
			case 0:
				message1 = "was capped in the ass by";
				break;
			case 1:
				message1 = "was gunned down in broad daylight by";
				break;
			case 2:
				message1 = "eats";
				message2 = "'s magnum";
				break;
			case 3:
				message1 = "was punked by";
				break;
			case 4:
				message1 = "was jacked by";
				break;
			default:
				message1 = "was killed by";
				message2 = "'s magnum";
				break;
			}
			break;
		}
		case MOD_SHOTGUN:   // Shotgun
		{
			num = rand() % 5;
			switch (num)
			{
			case 0:
				message1 = "is a victim of";
				message2 = "'s shotgun";
				break;
			case 1:
				message1 = "was slaughtered by";
				break;
			case 2:
				message1 = "drowns in a rain of";
				message2 = "'s hot lead";
				break;
			case 3:
				message1 = "finds out why";
				message2 = " got an F in gun safety";
				break;
			case 4:
				message1 = "was shotgunned by";
				break;
			default:
				message1 = "was killed by";
				message2 = "'s shotgun";
				break;
			}
			break;
		}
		case MOD_ASSRIFLE:
		{
			num = rand() % 4;
			switch (num)
			{
			case 0:
				message1 = "was assaulted by";
				break;
			case 1:
				message1 = "was cut in half by";
				break;
			case 2:
				message1 = "forgot that";
				message2 = " had a bigger gun";
				break;
			case 3:
				message1 = "was machinegunned down by";
				break;
			default:
				message1 = "was killed by";
				message2 = "'s assault rifle";
				break;
			}
		}
		break;
		case MOD_CHAINGUN:
			message1 = "was mowed down by";
			message2 = "'s chaingun";
			break;
		case MOD_GRENADE:
			message1 = "was fragged by";
			message2 = "'s grenade";
			break;
		case MOD_THRALLBALL:
			message1 = "ate";
			message2 = "'s ThrallBall";
			break;
		case MOD_THRALLSPLASH:
			message1 = "was bathed in";
			message2 = "'s ThrallBall";
			break;
		case MOD_ROCKET:
		{
			num = rand() % 5;
			switch (num)
			{
			case 0:
				message1 = "was blown up by";
				message2 = "'s direct rocket hit";
				break;
			case 1:
				message1 = "bows down to";
				message2 = "'s almighty rocket launcher";
				break;
			case 2:
				message1 = "was incinerated by";
				message2 = "'s rocket";
				break;
			case 3:
				message1 = "was obliterated by";
				message2 = "'s rocket";
				break;
			case 4:
				message1 = "likes walking into";
				message2 = "'s projectiles";
				break;
			default:
				message1 = "was killed by";
				message2 = "'s rocket launcher";
			}
		}
		break;
		case MOD_ROCKETSPLASH:
		{
			num = rand() % 5;
			switch (num)
			{
			case 0:
				message1 = "was caught in";
				message2 = "'s explosion";
				break;
			case 1:
				message1 = "was burned to a crisp in";
				message2 = "'s blast";
				break;
			case 2:
				message1 = "is pulling shrapnel out of his ass thanks to";
				break;
			case 3:
				message1 = "couldn't dodge";
				message2 = "'s blast damage";
				break;
			case 4:
				message1 = "was drenched by";
				message2 = "'s splash damage";
				break;
			default:
				message1 = "was killed by";
				message2 = "'s explosion";
			}
			break;
		}
		case MOD_SPIDERSPLASH:
			message1 = "was bitten by";
			message2 = "'s spidermine";
			break;
		case MOD_SPEARGUN:
			message1 = "was stuck by";
			message2 = "'s spear";
			break;
		case MOD_PULSE:
			message1 = "was pulsed by";
			break;
		case MOD_PULSELASER:
			message1 = "was burned to a crisp by";
			message2 = "'s laser";
			break;
		case MOD_ION:
		case MOD_ION_DESTRUCT:
			message1 = "Was ionized by";
			break;
		case MOD_QUANTUM:
			message1 = "Was particlized by";
			break;
		case MOD_SNIPER:
		{
			num = rand() % 5;
			switch (num)
			{
			case 0:
				message1 = "was hollowpointed by";
				break;
			case 1:
				message1 = "was put to sleep by";
				break;
			case 2:
				message1 = "was sniped by";
				break;
			case 3:
				message1 = "never saw it coming from";
				break;
			case 4:
				message1 = "was killed by a precision shot from";
				break;
			default:
				message1 = "was killed by";
				message2 = "'s sniper rifle";
			}
			break;
		}
		case MOD_EMPATHY:
			message1 = "felt";
			message2 = "'s pain";
			break;
		case MOD_GRAPPLE:
			message1 = "got stuck on";
			message2 = "'s grapple";
			break;
		case MOD_FRIENDLY_FIRE:
			message1 = "was killed by teammate";
			break;
		case MOD_CTFTURRET:
			message1 = "was caught in the sights of";
			message2 = "'s turret";
			break;
		default:
			message1 = "was killed by";
		}

		Com_Printf( "%s %s %s%s\n",
		            vi->name,
		            message1,
		            ai->name,
		            message2 );

		return;
	}
}

/*
==========================
CONSOLE_ParseCommandString
==========================
*/
void CONSOLE_ParseCommandString( const char *s, int type )
{
	const char *token;

	if (!console3d_init)
		CONSOLE_Init();

	// ================
	// CONSOLE COMMANDS
	// ================

	// Parse the rest of the commands left.
	while (s)
	{
		token = COM_Parse ( &s );

		if (!strcmp( token, "dm" ))
		{
			CONSOLE_DeathMessage( s );
			continue;
		}
		if (!strcmp( token, "iml" ))
		{
			CONSOLE_LoadIntermissionLayout( s );
			continue;
		}
		if (!strcmp( token, "imf" ))
		{
			CONSOLE_LoadIntermissionFile( s );
			continue;
		}
		if (!strcmp( token, "fi" ))
		{
			CONSOLE_FadeIn( s );
			continue;
		}
		if (!strcmp( token, "fo" ))
		{
			CONSOLE_FadeOut( s );
			continue;
		}
		if (!strcmp( token, "pl" ))
		{
			CONSOLE_ParsePickupList( s );
			continue;
		}
		if (!strcmp( token, "dia" ))
		{
			CONSOLE_DialogPrint( s );
			continue;
		}
		if (!strcmp( token, "lo" ))
		{
			CONSOLE_LoadGenericOverlay( s );
			continue;
		}
		if (!strcmp( token, "con" ))
		{
			CONSOLE_Create( s,false,false );
			continue;
		}
		if (!strcmp( token, "scon" ))
		{
			CONSOLE_Create( s,true,false );
			continue;
		}
		if (!strcmp( token, "mcon" ))
		{
			CONSOLE_Create( s,false,true );
			continue;
		}
		if (!strcmp( token, "mscon" ))
		{
			CONSOLE_Create( s,true,true );
			continue;
		}
		if (!strcmp( token, "spr" ))
		{
			float time = Sinatof( COM_Parse( &s ));
			CONSOLE_ScreenPrint( s,time );
			continue;
		}
		if (!strcmp( token, "spf" ))
		{
			CONSOLE_PrintFile( s );
			continue;
		}
		if (!strcmp( token, "cspf" ))
		{
			CONSOLE_ClearPrintFile( s );
			continue;
		}
		if (!strcmp( token, "act" ))
		{
			CONSOLE_Activate( s );
			continue;
		}
		if (!strcmp( token, "deact" ))
		{
			CONSOLE_Deactivate( s );
			continue;
		}
		if (!strcmp( token, "use" ))
		{
			CONSOLE_Use( s );
			continue;
		}
		if (!strcmp( token, "cp" ))
		{
			CONSOLE_Print( s );
			continue;
		}
		if (!strcmp( token, "cl" ))
		{
			CONSOLE_Layout( s,false );
			continue;
		}
		if (!strcmp( token, "cla" ))
		{
			CONSOLE_Layout( s,true );
			continue;
		}
		if (!strcmp( token, "clf" ))
		{
			CONSOLE_LayoutFile( s );
			continue;
		}
		if (!strcmp( token, "vw" ))
		{
			CONSOLE_SetVirtualWidth( s );
			continue;
		}
		if (!strcmp( token, "vh" ))
		{
			CONSOLE_SetVirtualHeight( s );
			continue;
		}
		if (!strcmp( token, "cf" ))
		{
			CONSOLE_SetFraction( s );
			continue;
		}
		if (!strcmp( token, "cols" ))
		{
			CONSOLE_SetColumns( s );
			continue;
		}
		if (!strcmp( token, "rows" ))
		{
			CONSOLE_SetRows( s );
			continue;
		}
		if (!strcmp( token, "clr" ))
		{
			CONSOLE_Clear( s );
			continue;
		}
		if (!strcmp( token, "lmf" ))
		{
			CONSOLE_LoadMenu( s );
			continue;
		}
		if (!strcmp( token, "foc" ))
		{
			CONSOLE_SetFocus( s );
			continue;
		}
		if (!strcmp( token, "cfg" ))
		{
			CONSOLE_SetForeground( s );
			continue;
		}
		if (!strcmp( token, "mia" ))
		{
			CONSOLE_SetMenuState( s,false );
			continue;
		}
		if (!strcmp( token, "ma" ))
		{
			CONSOLE_SetMenuState( s,true );
			continue;
		}
		if (!strcmp( token, "sb" ))
		{
			CONSOLE_SetStatusBar( s );
			continue;
		}
		if (!strcmp( token, "sbv" ))
		{
			CONSOLE_SetStatusBarValue( s );
			continue;
		}
		if (!strcmp( token, "sku" ))
		{
			CONSOLE_ScriptKickUsers( s );
			continue;
		}
		if (!strcmp( token, "pku" ))
		{
			CONSOLE_PlayerKickUsers( s );
			continue;
		}
	}
}

/*
=================
CONSOLE_DrawFades
=================
*/
void CONSOLE_DrawFades( void )
{
	static qboolean consolefaded = false;

	con_fadetime_off -= cls.renderFrametime;

	if ((con_fadetime_off > 0.0f) && (cls.state == ca_disconnected)) /* FS: Hey, if we're disconnected stop doing the fade */
	{
		con_fadetime_off = 0.0f;
		con_fade_direction = 1;
		consolefaded = true;
	}

	if ((con_fadetime_off <= 0) && (con_fade_direction > 0))
	{
		if ( consolefaded )
		{
			consolefaded = false;
			re.DrawFadeScreen( 0,0,0,0 );
		}
		return;
	}

	if (con_fadetime_start > 0)
		con_fade_a = con_fadetime_off / con_fadetime_start;

	if (con_fade_direction < 0)
		con_fade_a = 1.0f - con_fade_a;

	if (con_fade_a < 0)
		con_fade_a = 0;

	if (con_fade_a > 1)
		con_fade_a = 1;

	consolefaded = true;

	re.DrawFadeScreen( con_fade_r, con_fade_g, con_fade_b, con_fade_a );
}

/*
====================
CONSOLE_TestFadeIn_f
====================
*/
static void CONSOLE_TestFadeIn_f( void )
{

	if (Cmd_Argc() != 5) {
		Com_Printf( "usage: con_testfadein time r g b\n" );
		return;
	}

	con_fadetime_off   = Sinatof( Cmd_Argv( 1 ));
	con_fadetime_start   = con_fadetime_off;
	con_fade_r         = Sinatof( Cmd_Argv( 2 ));
	con_fade_g         = Sinatof( Cmd_Argv( 3 ));
	con_fade_b         = Sinatof( Cmd_Argv( 4 ));
	con_fade_direction = 1;
}

/*
=====================
CONSOLE_TestFadeOut_f
=====================
*/
static void CONSOLE_TestFadeOut_f( void )
{
	if (Cmd_Argc() != 5) {
		Com_Printf( "usage: con_testfadeout time r g b\n" );
		return;
	}
	con_fadetime_off   = Sinatof( Cmd_Argv( 1 ));
	con_fadetime_start = con_fadetime_off;
	con_fade_r         = Sinatof( Cmd_Argv( 2 ));
	con_fade_g         = Sinatof( Cmd_Argv( 3 ));
	con_fade_b         = Sinatof( Cmd_Argv( 4 ));
	con_fade_direction = -1;
}

/*
===================
CONSOLE_ClearFade_f
===================
*/
void CONSOLE_ClearFade_f( void )
{
	// haven't started the current fade yet
	if ( ( con_fadetime_off > 0 )&& ( con_fadetime_start == con_fadetime_off ) )
		return;
	con_fadetime_off   = 0;
	con_fade_direction = 1;
}

/*
======================
CONSOLE_ClearFadeExtra
======================
*/
void CONSOLE_ClearFadeExtra( void )
{
	if ( re.DrawFadeScreen )
		re.DrawFadeScreen( 0,0,0,0 );
}

/*
========================
CONSOLE_DrawGlobalLayout
========================
*/
void CONSOLE_DrawGlobalLayout( void )
{
	console3d_t    *p;
	const char     *s;
	float a;

	con_centertime_off -= cls.renderFrametime;

	if (con_centertime_off <= 0)
		return;

	p = CONSOLE_FindConsole( MAIN_CONSOLE );

	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_DrawGlobalLayout: Tried to draw a non-existent console: maincon\n");
		return;
	}

	s = p->LayoutPtr;
	a = con_centertime_off / 1.0f;

	if (a > 1.0f)
		a = 1.0f;

	if (s[0])
		DrawLayout3D( conva( "fc 1 1 1 %f %s",a,s ), p, -1 );
}

/*
===================
CONSOLE_CenterPrint - This is used for displaying entity bound
messages (I.e. you need this item to..." and for some help menu stuff.
===================
*/
void CONSOLE_CenterPrint( const char *string )
{
	assert( cls.mainconsole );
	CONSOLE_ResetTypeTimer( cls.mainconsole );
	strcpy( cls.mainconsole->LayoutPtr, string );
	con_centertime_off = scr_centertime->value;
	con_centertime_start = cl.time;
}

/*
===================
CONSOLE_ScreenPrint - This is used for displaying stuff from the script
===================
*/
void CONSOLE_ScreenPrint( const char *string, float time )
{
	assert( cls.mainconsole );
	CONSOLE_ResetTypeTimer( cls.mainconsole );
	strcpy( cls.mainconsole->LayoutPtr, string );
	con_centertime_off = time;
	con_centertime_start = cl.time;
}

/*
=====================
CONSOLE_CenterPrint_f
=====================
*/
static void CONSOLE_CenterPrint_f( void )
{
	if (Cmd_Argc() == 2) {
		CONSOLE_CenterPrint( Cmd_Argv( 1 ));
	} else {
		Com_Printf( "Usage: con_centerprint <layout string>\n" );
	}
}

/*
===================
CONSOLE_CheckResize
===================
*/
void CONSOLE_Resize( console3d_t *p )
{
	assert( p );

	if (viddef.width < 640 && viddef.height < 480)
	{
		p->columns        = viddef.width  / 8.0;
		p->rows           = viddef.height / 8.0;
		p->virtual_width  = viddef.width;
		p->virtual_height = viddef.height;
		p->oovw           = 1.0f / p->virtual_width;
		p->oovh           = 1.0f / p->virtual_height;
		p->offsetx        = (viddef.width  - 320.0f) / 2.0f;
		p->offsety        = (viddef.height - 240.0f) / 2.0f;
	}
	else if (viddef.width >= 640 && viddef.height >= 480)
	{
		p->columns        = viddef.width  / 8.0;
		p->rows           = viddef.height / 8.0;
		p->virtual_width  = viddef.width;
		p->virtual_height = viddef.height;
		p->oovw           = 1.0f / p->virtual_width;
		p->oovh           = 1.0f / p->virtual_height;
		p->offsetx        = (viddef.width  - 640.0f) / 2.0f;
		p->offsety        = (viddef.height - 480.0f) / 2.0f;
	}
}

/*
===================
CONSOLE_DialogPrint
===================
*/
void CONSOLE_DialogPrint( const char *string )
{
	char        *src, *dst;
	char dialog_src[1024];
	int i,len;
	int linex=0;
	char c;
	int wordcount=0;

	// String has the imageindex of the thing that is speaking,
	// and the textual portion.

	// Get the icon index
	dialog_icon = atoi( COM_Parse( &string ) );

	// Get the dialog string
	strcpy( dialog_src, COM_Parse( &string ) );

	src = dialog_src;
	dst = dialog_string;

	CONSOLE_DialogConsolePrint ( dialog_src, dialog_icon ); /* FS: Added */

	// Get wordcount
	for (i=0; i<strlen( dialog_src ); i++)
	{
		if(dialog_src[i] == ' ')
		{
			wordcount++;
		}
	}

	// Word wrap the buffer
	while ( (c = *src) )
	{
		for (len=0; len<scr_dialogwidth->value; len++)
		{
			if ( src[len] <= ' ')
			{
				break;
			}
		}

		if (len != scr_dialogwidth->value && (linex + len > scr_dialogwidth->value) )
		{
			linex = 0;
			*(dst++) = '\n';
		}

		*(dst++) = c;
		linex++;
		src++;
	}
	*dst = 0;

	con_diatime_off   = 0.7f * __max( 6, wordcount+1 );
	con_diatime_start = cl.time;
}

/*
==========================
CONSOLE_DrawGenericOverlay
==========================
*/
void CONSOLE_DrawGenericOverlay( void )
{
	console3d_t *p;
	const char *s;

	if (cls.state != ca_active || !cl.refresh_prepped)
		return;

	p = CONSOLE_FindConsole( MAIN_CONSOLE );

	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_DrawGenericOverlay: Tried to draw non-existent console: maincon\n");
		return;
	}

	s = (const char *) generic_overlay;
	if ( s && s[0] )
		DrawLayout3D( s,p,-1 );
}

/*
=======================
CONSOLE_DrawPrintScreen
=======================
*/
void CONSOLE_DrawPrintScreen( void )
{
	console3d_t *p;
	const char *s;

	if (cls.state != ca_active || !cl.refresh_prepped)
		return;

	p = CONSOLE_FindConsole( MAIN_CONSOLE );

	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_DrawPrintScreen: Tried to draw non-existent console: maincon\n");
		return;
	}

	s = (const char *) file_data;
	if ( s && s[0] )
		DrawLayout3D( s,p,-1 );
}

/*
==================
CONSOLE_DrawDialog
==================
*/
void CONSOLE_DrawDialog( void )
{
	console3d_t *p;

	con_diatime_off -= cls.renderFrametime;

	if (con_diatime_off <= 0)
		return;

	if (cls.state != ca_active || !cl.refresh_prepped)
		return;

	p = CONSOLE_FindConsole( MAIN_CONSOLE );

	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_DrawDialog: Tried to draw non-existent console: maincon\n");
		return;
	}

	if (viddef.width < 640 && viddef.height < 480)
		DrawLayout3D( "xl 10 yb 22 dialogpic xl 76 yb 22 dialogstring", p, -1 );
	else
		DrawLayout3D( "xl 140 yb 22 dialogpic xl 208 yb 22 dialogstring", p, -1 );
}

/*
===============
CONSOLE_DrawHUD
===============
*/
void CONSOLE_DrawHUD( void )
{
	console3d_t *p;
	const char *s;

	if (cls.state != ca_active || !cl.refresh_prepped)
		return;

	if ( !cls.hud )
		return;

	p = CONSOLE_FindConsole( MAIN_CONSOLE );

	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_DrawHUD: Tried to draw non-existent console: maincon\n");
		return;
	}

	if (scr_viewsize->value > 100)
		return;

	s = cls.hud;
	DrawLayout3D( s,p,-1 );

	SCR_DirtyScreen();
}

/*
===========================
CONSOLE_DrawMissionComputer
===========================
*/
void CONSOLE_DrawMissionComputer( void )
{
	console3d_t *p;
	const char *s;

	if (cls.state != ca_active || !cl.refresh_prepped)
		return;

	p = CONSOLE_FindConsole( MISSION_CONSOLE );

	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_DrawMissionComputer: Tried to draw non-existent console: missioncon\n");
		return;
	}

	CONSOLE_Resize( p );
	DrawLayout3D( "jcx jcy box 30 20 jcx yv 88 boxstring \"Mission Status\"",p,-1 );

	s = p->LayoutPtr;

	if (s[0])
		DrawLayout3D( s,p,-1 );
	else
		DrawLayout3D( "jcx jcy bstring \"No Mission Objectives\"",p,-1 );
}

/*
======================
CONSOLE_DrawSpectator
======================
*/
void CONSOLE_DrawSpectator( void )
{
	console3d_t *p;
	const char *s;

	if (cls.state != ca_active || !cl.refresh_prepped)
		return;

	p = CONSOLE_FindConsole( MAIN_CONSOLE );

	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_DrawSpectator: Tried to draw non-existent console: maincon\n");
		return;
	}

	s = cl.layout;
	DrawLayout3D( s,p,-1 );
}

/*
=========================
CONSOLE_DrawLoadingStatus
=========================
*/
void CONSOLE_DrawLoadingStatus( void )
{
	console3d_t    *p;
	const char     *s;
	//int percentage;
	int picnum=0;

	p = CONSOLE_FindConsole( MAIN_CONSOLE );

	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_DrawLoadingStatus: Tried to draw non-existent console: maincon\n");
		return;
	}

	// Calculate percentage of loaded
	cl_loadbar_percentage = ( ( float ) cl_status_current / ( float ) cl_status_total );

	if ( cl_loadbar_percentage > 1.0f )
		cl_loadbar_percentage = 1.0f;

	if ( cl_loadbar_percentage < 0 )
		cl_loadbar_percentage = 0;

	// Draw the default loading screen, if there is user defined stuff, then it
	// will get overdrawn on top of this
	//percentage = 290 * cl_loadbar_percentage;

	if ( con_vidres == CON_HIRES )
		s = va( "fullfill 0 0 0 1 oxl 0 oyb 256 picn im_frames/imep%i_1 oxl 256 oyb 256 picn im_frames/imep%i_2 oxl 512 oyb 256 picn im_frames/imep%i_3 oxl 0 oyb 0 picn im_frames/imep%i_4 oxl 256 oyb 0 picn im_frames/imep%i_5 oxl 512 oyb 0 picn im_frames/imep%i_6 oxl 0 oyb 0 loadbar 640 8", picnum, picnum, picnum, picnum, picnum, picnum );
	else
		s = va( "fullfill 0 0 0 1 oxl 0 oyb 0 picn im_frames/im_default_1 oxl 256 oyb 0 picn im_frames/im_default_2 oxl 0 oyb 0 loadbar 320 8" );

	DrawLayout3D( s,p,-1 );

	if ( cls.download )
	{
		if ( con_vidres == CON_HIRES )
			s = va( "oxl 0 oyb 8 downloadbar 640 8 jcx string2 \"Downloading: %s\"", cls.downloadname );
		else
			s = va( "oxl 0 oyb 8 downloadbar 320 8 jcx string2 \"Downloading: %s\"", cls.downloadname );

		DrawLayout3D( s,p,-1 );
	}

	// Draw the intermission data if there is one.
	if ( intermission_data ) {
		DrawLayout3D( (char *) intermission_data, p, -1 );
	} else {
		// Check for default intermission basd on the map file name
		if (intermission_layoutfile[0]) {
			char tempfile[MAX_OSPATH];

			COM_StripExtension( intermission_layoutfile, tempfile );
			strcpy( intermission_layoutfile, tempfile );

			if ( con_vidres == CON_HIRES )
				COM_DefaultExtension( intermission_layoutfile, ".imh" );
			else
				COM_DefaultExtension( intermission_layoutfile, ".iml" );

			FS_LoadFile( intermission_layoutfile, &intermission_data );

			if ( intermission_data ) {
				DrawLayout3D( (char *) intermission_data, p, -1 );
			} else {
				intermission_layoutfile[0] = 0;
			}
		}
	}
	if ( intermission_layout ) {
		DrawLayout3D( intermission_layout, p, -1 );
	}
}

/*
==================
CONSOLE_DrawScores
==================
*/
void CONSOLE_DrawScores( void )
{
	console3d_t *p;
	const char *s;

	if (cls.state != ca_active || !cl.refresh_prepped)
		return;

	p = CONSOLE_FindConsole( MAIN_CONSOLE );

	if (!p)
	{
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "CONSOLE_DrawScores: Tried to draw non-existent console: maincon\n");
		return;
	}

	client_count = 0;
	s = cl.layout;
	if ( s && s[0])
	{
		DrawLayout3D( s,p,-1 );
	}
}

/*
======================
CONSOLE_ResetTypeTimer
======================
*/
void CONSOLE_ResetTypeTimer( console3d_t *p )
{
	p->type_index = 1;
	c_type_starttime = cls.realtime;
}

/*
==================
CONSOLE_ResetTimer
==================
*/
void CONSOLE_ResetTimer( console3d_t *p )
{
	c_starttime = cls.realtime;
}

/*
===================
CONSOLE_CheckResize
===================
*/
void CONSOLE_CheckResize( void )
{
	char hudfile[MAX_OSPATH];

	if (!cls.mainconsole)
		return;

	// Set the main console depending on the screen resolution
	if ( viddef.width < 640 && viddef.height < 480 )
	{
		// Set values for resolution
		cls.mainconsole->columns = viddef.width  / 8.0;
		cls.mainconsole->rows    = viddef.height / 8.0;
		cls.mainconsole->virtual_width  = viddef.width;
		cls.mainconsole->virtual_height = viddef.height;
		cls.mainconsole->oovw = 1.0f / cls.mainconsole->virtual_width;
		cls.mainconsole->oovh = 1.0f / cls.mainconsole->virtual_height;
		cls.mainconsole->offsetx = (viddef.width  - 320.0f) / 2.0f;
		cls.mainconsole->offsety = (viddef.height - 240.0f) / 2.0f;
	}
	else if ( viddef.width >= 640 && viddef.height >= 480 )
	{
		cls.mainconsole->columns = viddef.width  / 8.0;
		cls.mainconsole->rows    = viddef.height / 8.0;
		cls.mainconsole->virtual_width  = viddef.width;
		cls.mainconsole->virtual_height = viddef.height;
		cls.mainconsole->oovw = 1.0f / cls.mainconsole->virtual_width;
		cls.mainconsole->oovh = 1.0f / cls.mainconsole->virtual_height;
		cls.mainconsole->offsetx = (viddef.width  - 640.0f) / 2.0f;
		cls.mainconsole->offsety = (viddef.height - 480.0f) / 2.0f;
	}

	// Switching from HIRES to LORES
	if ( ( con_vidres == CON_HIRES && viddef.width < 640 ) ||
	     ( con_vidres == CON_NORES && viddef.width < 640 ) )
	{
		// Load lores menu
		CONSOLE_LoadMenu( "maincon menus/main_alt.mnu" );
		CONSOLE_SetKeyFunction( cls.mainconsole );

		// Fix the dialog width
		Cvar_SetValue( "scr_dialogwidth", 22 );

		// Load the alternate hud
		COM_StripExtension( Cvar_VariableString( "cl_hudfile" ),hudfile );
		COM_DefaultExtension( hudfile,".hdl" );
		Cvar_Set( "cl_hudfile", hudfile );

		con_vidres = CON_LORES;
		HUD_Restart_f();

		if (cls.currentconsole == cls.mainconsole)
		{
			CONSOLE_Exit( cls.mainconsole );
			MENU_Main_f();
		}
	}
	else if ( ( con_vidres == CON_LORES && viddef.width >= 640 ) ||
	          ( con_vidres == CON_NORES && viddef.width >= 640 ) )
	{
		// Load hires menu
		CONSOLE_LoadMenu( "maincon menus/main.mnu" );
		CONSOLE_SetKeyFunction( cls.mainconsole );

		// Fix dialog width
		Cvar_SetValue( "scr_dialogwidth", 30 );

		// Load the alternate hud
		COM_StripExtension( Cvar_VariableString( "cl_hudfile" ),hudfile );
		COM_DefaultExtension( hudfile,".hdh" );
		Cvar_Set( "cl_hudfile", hudfile );

		con_vidres = CON_HIRES;
		HUD_Restart_f();

		if (cls.currentconsole == cls.mainconsole)
		{
			CONSOLE_Exit( cls.mainconsole );
			MENU_Main_f();
		}
	}
}

/*
===============
CONSOLE_Process
===============
*/
void CONSOLE_Process( const char *con_name )
{
	const char *s;
	console3d_t *p;

	p = CONSOLE_FindConsole( con_name );

	// Console is not created yet.
	if (!p)
		return;

	// Console is not active
	if (!p->active)
		return;

	// Parse the layout
	s = p->LayoutPtr;
	DrawLayout3D( s,p,-1 );

	// Draw menus
	if (p->menucon && p->drawmenu)
	{
		if ( p->numlevels )
			CONSOLE_DrawMenu3D( p );
	}

	// If this is a scrolling console. update it.
	if (p->scrollcon)
	{
		CONSOLE_DrawConsole3D( p );
		if (p->focus == CONSOLE3D)
		{
			CONSOLE_DrawInput3D( p );
		}
	}
}


static void CONSOLE_Precache( void )
{
	const char     **tgafiles;
	int i,nfiles;

	// Precache all the images in the pics directory
	tgafiles = FS_FindFiles( "pics/", "*.tga", &nfiles );

	if ( !tgafiles )
		return;

	for ( i = 0; i < nfiles; i++ ) {
		const char * base = COM_SkipPath( tgafiles[i] );
		if ( base[0] && base[0] != '_' )
			re.RegisterPic ( base );
	}
	FS_FreeList( tgafiles, nfiles );
}

void CON_Init( void )
{
	int w,h;
	byte     *pal;
	qboolean hasalpha;

	MENU_InitPlayerConfig();
	MENU_InitDeathmatchMapsConfig();
	CONSOLE_Init ();
	MENU_Init ();
	HUD_Init ( con_vidres );

	scr_dialogwidth = Cvar_Get( "scr_dialogwidth", "30", 0 );
	con_printdialog = Cvar_Get( "con_printdialog", "0", CVAR_ARCHIVE ); /* FS */
	//con_printdialog->description = "Print in-game dialog to the console buffer.";

	Cmd_AddCommand( "con_centerprint", CONSOLE_CenterPrint_f );
	Cmd_AddCommand( "con_fadein", CONSOLE_TestFadeIn_f );
	Cmd_AddCommand( "con_fadeout", CONSOLE_TestFadeOut_f );
	Cmd_AddCommand( "con_clearfade", CONSOLE_ClearFade_f );
	Cmd_AddCommand( "con_clearintermission", CONSOLE_ClearIntermissionData_f );
	Cmd_AddCommand( "con_beginlevel", CONSOLE_BeginLevel_f );
	Cmd_AddCommand( "menu_reload", MENU_Reload_f );

	pal = 0;
	free( armor_lookup );
	armor_lookup = 0;
	if (!LoadTGA( "pics/armordamage.tga",&armor_lookup,&w,&h,&pal,&hasalpha ))
		Com_DPrintf(DEVELOPER_MSG_CONSOLE3D, "Could not load armordamage.tga\n");

	free( pal );
	if ( precache && precache->value )
		CONSOLE_Precache();

	CONSOLE_CheckResize();
}

void CON_Destroy( void )
{
	free( armor_lookup );
	armor_lookup = 0;
	CONSOLE_ClearConsoles();        /* Release the memory */
	CONSOLE_Destroy( &genconsole );
	CONSOLE_Destroy( &mainconsole );
}

char *_strdate2( char *str )
{
	time_t timer;

	time( &timer );
	strftime( str, 10, "%m/%d/%y", localtime( &timer ));
	return str;
}


char *_strtime2( char *str )
{
	time_t timer;

	time( &timer );
	strftime( str, 10, "%I:%M:%S", localtime( &timer ));
	return str;
}

static void CONSOLE_DialogConsolePrint( const char *str, int icon ) /* FS: Added */
{
	if (!str || !str[0])
		return;

	if (((int)con_printdialog->value) && cl.configstrings[CS_IMAGES+icon][0])
	{
		if (Q_stricmp(cl.configstrings[CS_IMAGES+icon],"i_computer") != 0) /* FS: Don't output objectives updated message */
		{
			int x;
			int z = con.current;
			const char *dialogstr = cl.configstrings[CS_IMAGES+icon];

			if (!strncmp(dialogstr, "i_", 2))
				dialogstr += 2;

			Com_Printf("%s: %s\n", dialogstr, str);

			if (con.current >= 0)
			{
				for (x = z; x <= con.current; x++) {
					con.times[x % NUM_CON_TIMES] = -99999; /* FS: Don't show it at the top, looks dumb when this is happening simultaneously */
				}
			}
		}
	}
}
