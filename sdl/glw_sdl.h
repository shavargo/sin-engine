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
#ifndef __GLW_SDL_H__
#define __GLW_SDL_H__

#include <SDL2/SDL.h>
#include <stdio.h>

#ifndef _WIN32
#ifndef WINAPI
#define WINAPI
#endif
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef BOOL
typedef int BOOL;
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef HDC
typedef void *HDC;
#endif
#ifndef HWND
typedef void *HWND;
#endif
#ifndef HGLRC
typedef void *HGLRC;
#endif
#ifndef HINSTANCE
typedef void *HINSTANCE;
#endif
#ifndef PROC
typedef void *PROC;
#endif
#ifndef LPCSTR
typedef const char *LPCSTR;
#endif
#else
#include <windows.h>
#endif

typedef struct
{
	SDL_Window   *window;
	SDL_GLContext context;

	void         *hInstance;
	void         *wndproc;
	void         *hDC;
	void         *hWnd;
	void         *hGLRC;
	void         *hinstOpenGL;

	qboolean     minidriver;
	qboolean     allowdisplaydepthchange;
	qboolean     mcd_accelerated;

	FILE         *log_fp;
} glwstate_t;

extern glwstate_t glw_state;

#endif /* __GLW_SDL_H__ */
