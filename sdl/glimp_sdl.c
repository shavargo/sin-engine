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

#include "../ref_gl/gl_local.h"
#include "glw_sdl.h"

#ifdef _WIN32
#include <SDL2/SDL_syswm.h>
HWND cl_hwnd = NULL;
#endif

glwstate_t glw_state;

extern cvar_t *vid_fullscreen;
extern cvar_t *vid_ref;

qboolean GLimp_Init( void *hinstance, void *wndproc )
{
	(void)hinstance;
	(void)wndproc;

	if ( SDL_WasInit( SDL_INIT_VIDEO ) == 0 )
	{
		if ( SDL_InitSubSystem( SDL_INIT_VIDEO ) < 0 )
		{
			ri.Con_Printf( PRINT_ALL, "GLimp_Init() - SDL_InitSubSystem(VIDEO) failed: %s\n", SDL_GetError() );
			return false;
		}
	}

	return true;
}

rserr_t GLimp_SetMode( int *pwidth, int *pheight, int mode, qboolean fullscreen )
{
	int width, height;

	ri.Con_Printf( PRINT_ALL, "Initializing SDL2 OpenGL display\n" );
	ri.Con_Printf( PRINT_ALL, "...setting mode %d (fullscreen: %d)\n", mode, fullscreen );

	if ( mode == -1 )
	{
		cvar_t *cw = ri.Cvar_Get( "r_customwidth", "1280", CVAR_ARCHIVE );
		cvar_t *ch = ri.Cvar_Get( "r_customheight", "720", CVAR_ARCHIVE );
		width = (int)cw->value;
		height = (int)ch->value;
	}
	else
	{
		if ( !ri.Vid_GetModeInfo( &width, &height, mode ) )
		{
			ri.Con_Printf( PRINT_ALL, "GLimp_SetMode() - invalid mode %d\n", mode );
			return rserr_invalid_mode;
		}
	}

	// Destroy existing window and context if changing mode
	if ( glw_state.context )
	{
		SDL_GL_DeleteContext( glw_state.context );
		glw_state.context = NULL;
	}
	if ( glw_state.window )
	{
		SDL_DestroyWindow( glw_state.window );
		glw_state.window = NULL;
	}

	// Set GL context attributes (Desktop OpenGL 2.1 compatibility profile for classic immediate-mode rendering)
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 24 );
	SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

	Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
	if ( fullscreen )
	{
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	glw_state.window = SDL_CreateWindow( "SiN", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags );
	if ( !glw_state.window )
	{
		ri.Con_Printf( PRINT_ALL, "GLimp_SetMode() - SDL_CreateWindow failed: %s\n", SDL_GetError() );
		return rserr_invalid_mode;
	}

#ifdef _WIN32
	{
		SDL_SysWMinfo wmInfo;
		SDL_VERSION( &wmInfo.version );
		if ( SDL_GetWindowWMInfo( glw_state.window, &wmInfo ) )
		{
			glw_state.hWnd = wmInfo.info.win.window;
			cl_hwnd = (HWND)wmInfo.info.win.window;
		}
	}
#endif

	glw_state.context = SDL_GL_CreateContext( glw_state.window );
	if ( !glw_state.context )
	{
		ri.Con_Printf( PRINT_ALL, "GLimp_SetMode() - SDL_GL_CreateContext failed: %s\n", SDL_GetError() );
		SDL_DestroyWindow( glw_state.window );
		glw_state.window = NULL;
		return rserr_invalid_mode;
	}

	SDL_GL_MakeCurrent( glw_state.window, glw_state.context );

	cvar_t *vsync = ri.Cvar_Get( "r_vsync", "1", CVAR_ARCHIVE );
	if ( vsync )
	{
		SDL_GL_SetSwapInterval( (int)vsync->value );
	}

	if ( !QGL_Init( "opengl32" ) )
	{
		ri.Con_Printf( PRINT_ALL, "GLimp_SetMode() - QGL_Init failed\n" );
		GLimp_Shutdown();
		return rserr_invalid_mode;
	}

	int dw = width, dh = height;
	SDL_GL_GetDrawableSize( glw_state.window, &dw, &dh );
	*pwidth = dw;
	*pheight = dh;

	gl_state.fullscreen = fullscreen;
	gl_state.prev_mode = mode;

	ri.Vid_NewWindow( *pwidth, *pheight );

	return rserr_ok;
}

void GLimp_Shutdown( void )
{
	QGL_Shutdown();

	if ( glw_state.context )
	{
		SDL_GL_DeleteContext( glw_state.context );
		glw_state.context = NULL;
	}
	if ( glw_state.window )
	{
		SDL_DestroyWindow( glw_state.window );
		glw_state.window = NULL;
	}
}

void GLimp_BeginFrame( float camera_separation )
{
	(void)camera_separation;
}

void GLimp_EndFrame( void )
{
	if ( glw_state.window )
	{
		SDL_GL_SwapWindow( glw_state.window );
	}
}

void GLimp_AppActivate( qboolean active )
{
	(void)active;
}
