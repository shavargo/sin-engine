/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2026 SiN MP Engine Contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.
*/

#include <SDL2/SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../client/client.h"

#ifdef _WIN32
#include <windows.h>
HWND cl_hwnd = NULL;
#endif

#define MAXPRINTMSG 4096

cvar_t *vid_ref;
cvar_t *vid_fullscreen;
cvar_t *vid_gamma;

refexport_t re;
refdef_t    refdef;
viddef_t    viddef;

static void *reflib_library = NULL;
static qboolean reflib_active = false;

/*
** VID_Printf
*/
static void VID_Printf( int print_level, char *fmt, ... )
{
	va_list argptr;
	char    msg[MAXPRINTMSG];

	va_start( argptr, fmt );
	vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	if ( print_level == PRINT_ALL )
		Com_Printf( "%s", msg );
	else if ( print_level == PRINT_DEVELOPER )
		Com_DPrintf( "%s", msg );
	else if ( print_level == PRINT_ALERT )
		Com_Printf( "ALERT: %s\n", msg );
}

/*
** VID_Error
*/
static void VID_Error( int err_level, char *fmt, ... )
{
	va_list argptr;
	char    msg[MAXPRINTMSG];

	va_start( argptr, fmt );
	vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	Com_Error( err_level, "%s", msg );
}

/*
** VID_NewWindow
*/
void VID_NewWindow( int width, int height )
{
	viddef.width = width;
	viddef.height = height;
	cl.force_refdef = true;
}

/*
** VID_FreeReflib
*/
void VID_FreeReflib( void )
{
	if ( reflib_library )
	{
		SDL_UnloadObject( reflib_library );
		reflib_library = NULL;
	}
	memset( &re, 0, sizeof( re ) );
	reflib_active = false;
}

/*
** Mode list
*/
typedef struct
{
	const char *description;
	int         width, height;
} vidmode_t;

static const vidmode_t vid_modes[] =
{
	{ "Mode 0: 320x240 (4:3)",      320,  240 },
	{ "Mode 1: 400x300 (4:3)",      400,  300 },
	{ "Mode 2: 512x384 (4:3)",      512,  384 },
	{ "Mode 3: 640x480 (4:3)",      640,  480 },
	{ "Mode 4: 800x600 (4:3)",      800,  600 },
	{ "Mode 5: 960x720 (4:3)",      960,  720 },
	{ "Mode 6: 1024x768 (4:3)",    1024,  768 },
	{ "Mode 7: 1152x864 (4:3)",    1152,  864 },
	{ "Mode 8: 1280x720 (16:9)",   1280,  720 },
	{ "Mode 9: 1280x800 (16:10)",  1280,  800 },
	{ "Mode 10: 1280x960 (4:3)",   1280,  960 },
	{ "Mode 11: 1280x1024 (5:4)",  1280, 1024 },
	{ "Mode 12: 1366x768 (16:9)",  1366,  768 },
	{ "Mode 13: 1440x900 (16:10)", 1440,  900 },
	{ "Mode 14: 1600x900 (16:9)",  1600,  900 },
	{ "Mode 15: 1600x1200 (4:3)",  1600, 1200 },
	{ "Mode 16: 1680x1050 (16:10)",1680, 1050 },
	{ "Mode 17: 1920x1080 (16:9)", 1920, 1080 },
	{ "Mode 18: 1920x1200 (16:10)",1920, 1200 },
	{ "Mode 19: 2560x1080 (21:9)", 2560, 1080 },
	{ "Mode 20: 2560x1440 (16:9)", 2560, 1440 },
	{ "Mode 21: 2560x1600 (16:10)",2560, 1600 },
	{ "Mode 22: 3440x1440 (21:9)", 3440, 1440 },
	{ "Mode 23: 3840x2160 (4K)",    3840, 2160 }
};

#define VID_NUM_MODES (int)(sizeof(vid_modes) / sizeof(vid_modes[0]))

int VID_GetModeCount( void )
{
	return VID_NUM_MODES;
}

const char *VID_GetModeDescription( int mode )
{
	if ( ( mode < 0 ) || ( mode >= VID_NUM_MODES ) )
		return " UNKNOWN";
	return vid_modes[mode].description;
}

qboolean VID_GetModeInfo( int *width, int *height, int mode )
{
	if ( mode < 0 || mode >= VID_NUM_MODES )
		return false;

	*width = vid_modes[mode].width;
	*height = vid_modes[mode].height;
	return true;
}

/*
** VID_LoadRefresh
*/
qboolean VID_LoadRefresh( char *name )
{
	refimport_t ri;
	GetRefAPI_t GetRefAPI;

	if ( reflib_active )
	{
		re.Shutdown();
		VID_FreeReflib();
	}

	Com_Printf( "------- Loading %s -------\n", name );

	reflib_library = SDL_LoadObject( name );
	if ( !reflib_library )
	{
		char fallback[MAX_OSPATH];
		Com_sprintf( fallback, sizeof( fallback ), "./%s", name );
		reflib_library = SDL_LoadObject( fallback );
	}
#if !defined(_WIN32)
	if ( !reflib_library )
	{
		char soname[MAX_OSPATH];
		Com_sprintf( soname, sizeof( soname ), "lib%s.so", name );
		reflib_library = SDL_LoadObject( soname );
	}
#endif

	if ( !reflib_library )
	{
		Com_Printf( "SDL_LoadObject(\"%s\") failed: %s\n", name, SDL_GetError() );
		return false;
	}

	GetRefAPI = (GetRefAPI_t)SDL_LoadFunction( reflib_library, "GetRefAPI" );
	if ( !GetRefAPI )
	{
		Com_Printf( "SDL_LoadFunction(\"GetRefAPI\") failed: %s\n", SDL_GetError() );
		VID_FreeReflib();
		return false;
	}

	ri.Cmd_AddCommand = Cmd_AddCommand;
	ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
	ri.Cmd_Argc = Cmd_Argc;
	ri.Cmd_Argv = Cmd_Argv;
	ri.Cmd_ExecuteText = Cbuf_ExecuteText;
	ri.Con_Printf = VID_Printf;
	ri.Sys_Error = VID_Error;
	ri.FS_LoadFile = FS_LoadFile;
	ri.FS_FreeFile = FS_FreeFile;
	ri.FS_Gamedir = FS_Gamedir;
	ri.Cvar_Get = Cvar_Get;
	ri.Cvar_Set = Cvar_Set;
	ri.Cvar_SetValue = Cvar_SetValue;
	ri.Vid_GetModeInfo = VID_GetModeInfo;
	ri.Vid_MenuInit = VID_MenuInit;
	ri.Vid_NewWindow = VID_NewWindow;

	re = GetRefAPI( ri );

	if ( re.api_version != API_VERSION )
	{
		VID_FreeReflib();
		Com_Printf( "%s has incompatible api_version (%d, expected %d)\n", name, re.api_version, API_VERSION );
		return false;
	}

	if ( re.Init( NULL, NULL ) == -1 )
	{
		re.Shutdown();
		VID_FreeReflib();
		return false;
	}

	Com_Printf( "------------------------------------\n" );
	reflib_active = true;
	return true;
}

void VID_Restart_f( void )
{
	vid_ref->modified = true;
}

void VID_Front_f( void )
{
	SDL_Window *win = SDL_GL_GetCurrentWindow();
	if ( win )
	{
		SDL_RaiseWindow( win );
	}
}

void VID_CheckChanges( void )
{
	char name[100];

	if ( vid_ref->modified )
	{
		cl.force_refdef = true;
		S_StopAllSounds();
	}

	while ( vid_ref->modified )
	{
		vid_ref->modified = false;
		vid_fullscreen->modified = true;
		cl.refresh_prepped = false;
		cls.disable_screen = true;

#if defined(_WIN32)
		Com_sprintf( name, sizeof( name ), "ref_%s.dll", vid_ref->string );
#elif defined(__APPLE__)
		Com_sprintf( name, sizeof( name ), "ref_%s.dylib", vid_ref->string );
#else
		Com_sprintf( name, sizeof( name ), "ref_%s.so", vid_ref->string );
#endif
		if ( !VID_LoadRefresh( name ) )
		{
			if ( strcmp( vid_ref->string, "gl" ) == 0 )
				Com_Error( ERR_FATAL, "Couldn't load ref_gl! Please ensure ref_gl is compiled and in the same directory." );

			Cvar_Set( "vid_ref", "gl" );
			if ( cls.key_dest != key_console )
				Con_ToggleConsole_f();
		}

		cls.disable_screen = false;
	}
}

void VID_Init( void )
{
	vid_ref = Cvar_Get( "vid_ref", "gl", CVAR_ARCHIVE );
	vid_fullscreen = Cvar_Get( "vid_fullscreen", "0", CVAR_ARCHIVE );
	vid_gamma = Cvar_Get( "vid_gamma", "1", CVAR_ARCHIVE );

	Cmd_AddCommand( "vid_restart", VID_Restart_f );
	Cmd_AddCommand( "vid_front", VID_Front_f );

	VID_CheckChanges();
}

void VID_Shutdown( void )
{
	if ( reflib_active )
	{
		re.Shutdown();
		VID_FreeReflib();
	}
}
