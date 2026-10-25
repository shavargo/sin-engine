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

#include "../client/client.h"
#include "../client/cdaudio.h"

int CDAudio_Init( void )
{
	return 0;
}

void CDAudio_Shutdown( void )
{
}

void CDAudio_Play( int track, qboolean looping )
{
	(void)track;
	(void)looping;
}

void CDAudio_Stop( void )
{
}

void CDAudio_Update( void )
{
}

void CDAudio_Activate( qboolean active )
{
	(void)active;
}
