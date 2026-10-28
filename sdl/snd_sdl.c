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
#include <stdlib.h>
#include <string.h>

#include "../client/client.h"
#include "../client/snd_loc.h"

extern dma_t dma;

static SDL_AudioDeviceID audio_device = 0;
static qboolean snd_inited = false;
static int snd_buffer_size = 0;
static int snd_read_pos = 0;

static void SDLCALL AudioCallback( void *userdata, Uint8 *stream, int len )
{
	(void)userdata;

	if ( !dma.buffer || !snd_inited )
	{
		memset( stream, 0, len );
		return;
	}

	int remaining = len;
	while ( remaining > 0 )
	{
		int chunk = snd_buffer_size - snd_read_pos;
		if ( chunk > remaining )
			chunk = remaining;

		memcpy( stream, dma.buffer + snd_read_pos, chunk );
		stream += chunk;
		snd_read_pos = ( snd_read_pos + chunk ) % snd_buffer_size;
		remaining -= chunk;
	}

	// Update DMA samplepos in mono samples
	dma.samplepos = snd_read_pos / ( dma.samplebits / 8 );
}

qboolean SNDDMA_Init( void )
{
	if ( snd_inited )
		return true;

	memset( &dma, 0, sizeof( dma ) );

	if ( SDL_WasInit( SDL_INIT_AUDIO ) == 0 )
	{
		if ( SDL_InitSubSystem( SDL_INIT_AUDIO ) < 0 )
		{
			Com_Printf( "SNDDMA_Init: SDL_InitSubSystem(AUDIO) failed: %s\n", SDL_GetError() );
			return false;
		}
	}

	SDL_AudioSpec desired, obtained;
	memset( &desired, 0, sizeof( desired ) );
	memset( &obtained, 0, sizeof( obtained ) );

	desired.freq = 44100;
	if ( s_khz && (int)s_khz->value == 22 )
		desired.freq = 22050;
	else if ( s_khz && (int)s_khz->value == 11 )
		desired.freq = 11025;

	desired.format = AUDIO_S16SYS;
	desired.channels = 2;
	desired.samples = 1024;
	desired.callback = AudioCallback;

	audio_device = SDL_OpenAudioDevice( NULL, 0, &desired, &obtained, 0 );
	if ( !audio_device )
	{
		Com_Printf( "SNDDMA_Init: SDL_OpenAudioDevice failed: %s\n", SDL_GetError() );
		return false;
	}

	dma.channels = obtained.channels;
	dma.samplebits = 16;
	dma.speed = obtained.freq;

	// Ring buffer: 64KB (~370ms at 44.1kHz 16-bit stereo)
	snd_buffer_size = 65536;
	dma.buffer = (byte *)malloc( snd_buffer_size );
	if ( !dma.buffer )
	{
		SDL_CloseAudioDevice( audio_device );
		audio_device = 0;
		return false;
	}
	memset( dma.buffer, 0, snd_buffer_size );

	dma.samples = snd_buffer_size / ( dma.samplebits / 8 );
	dma.submission_chunk = 1;
	dma.samplepos = 0;
	snd_read_pos = 0;

	snd_inited = true;

	// Unpause playback
	SDL_PauseAudioDevice( audio_device, 0 );

	Com_Printf( "SDL Audio initialized: %d Hz, %d channels, %d bits, ring buffer %d bytes\n",
				dma.speed, dma.channels, dma.samplebits, snd_buffer_size );

	return true;
}

int SNDDMA_GetDMAPos( void )
{
	return dma.samplepos;
}

void SNDDMA_BeginPainting( void )
{
	// Buffer is valid in continuous circular DMA mode
}

void SNDDMA_Submit( void )
{
	// Audio callback streams continuously
}

void SNDDMA_Shutdown( void )
{
	if ( audio_device )
	{
		SDL_PauseAudioDevice( audio_device, 1 );
		SDL_CloseAudioDevice( audio_device );
		audio_device = 0;
	}

	if ( dma.buffer )
	{
		free( dma.buffer );
		dma.buffer = NULL;
	}

	snd_inited = false;
	SDL_QuitSubSystem( SDL_INIT_AUDIO );
}
