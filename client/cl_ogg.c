/*
==============================================================================

Yamagi Quake II style Ogg Vorbis soundtrack streaming subsystem for SiN MP
Uses stb_vorbis for lightweight, dependency-free audio playback.

==============================================================================
*/

#include "client.h"
#include "sound.h"
#include "cl_ogg.h"

extern int s_rawend;
extern int paintedtime;

#define STB_VORBIS_NO_STDIO
#include "stb_vorbis.h"

static stb_vorbis	*ogg_stream = NULL;
static byte			*ogg_file_buffer = NULL;
static int			ogg_file_size = 0;
static qboolean		ogg_playing = false;
static qboolean		ogg_looping = false;
static int			ogg_current_track = -1;
static char			ogg_current_file[MAX_QPATH] = { 0 };

static cvar_t		*ogg_enable;
static cvar_t		*ogg_volume;
static cvar_t		*ogg_autoloop;

#define OGG_CHUNK_SAMPLES 2048
static short pcm_samples[OGG_CHUNK_SAMPLES * 2];

/*
==================
OGG_Stop
==================
*/
void OGG_Stop (void)
{
	if (ogg_stream)
	{
		stb_vorbis_close (ogg_stream);
		ogg_stream = NULL;
	}

	if (ogg_file_buffer)
	{
		FS_FreeFile (ogg_file_buffer);
		ogg_file_buffer = NULL;
	}

	ogg_file_size = 0;
	ogg_playing = false;
	ogg_current_track = -1;
	ogg_current_file[0] = '\0';
}

/*
==================
OGG_Pause
==================
*/
void OGG_Pause (void)
{
	ogg_playing = !ogg_playing;
}

/*
==================
OGG_IsPlaying
==================
*/
qboolean OGG_IsPlaying (void)
{
	return ogg_playing;
}

/*
==================
OGG_PlayFile
==================
*/
qboolean OGG_PlayFile (const char *filename, qboolean looping)
{
	int error = 0;

	if (!ogg_enable || !ogg_enable->value)
		return false;

	if (!filename || !filename[0])
		return false;

	OGG_Stop ();

	ogg_file_size = FS_LoadFile ((char *)filename, (void **)&ogg_file_buffer);
	if (!ogg_file_buffer || ogg_file_size <= 0)
	{
		return false;
	}

	ogg_stream = stb_vorbis_open_memory (ogg_file_buffer, ogg_file_size, &error, NULL);
	if (!ogg_stream)
	{
		Com_DPrintf ("OGG_PlayFile: stb_vorbis_open_memory failed on %s (error %d)\n", filename, error);
		FS_FreeFile (ogg_file_buffer);
		ogg_file_buffer = NULL;
		ogg_file_size = 0;
		return false;
	}

	ogg_playing = true;
	ogg_looping = looping;
	Q_strlcpy (ogg_current_file, filename, sizeof(ogg_current_file));
	Com_DPrintf ("OGG_PlayFile: playing %s (looping=%d)\n", filename, looping);
	return true;
}

/*
==================
OGG_Play
==================
*/
qboolean OGG_Play (int track, qboolean looping)
{
	char filename[MAX_QPATH];

	if (!ogg_enable || !ogg_enable->value)
		return false;

	if (track <= 0)
	{
		OGG_Stop ();
		return false;
	}

	if (ogg_autoloop && ogg_autoloop->value)
		looping = true;

	/* Try music/track%02i.ogg */
	Com_sprintf (filename, sizeof(filename), "music/track%02i.ogg", track);
	if (OGG_PlayFile (filename, looping))
	{
		ogg_current_track = track;
		return true;
	}

	/* Try music/track%i.ogg */
	Com_sprintf (filename, sizeof(filename), "music/track%i.ogg", track);
	if (OGG_PlayFile (filename, looping))
	{
		ogg_current_track = track;
		return true;
	}

	/* Try music/%02i.ogg */
	Com_sprintf (filename, sizeof(filename), "music/%02i.ogg", track);
	if (OGG_PlayFile (filename, looping))
	{
		ogg_current_track = track;
		return true;
	}

	/* Try music/%i.ogg */
	Com_sprintf (filename, sizeof(filename), "music/%i.ogg", track);
	if (OGG_PlayFile (filename, looping))
	{
		ogg_current_track = track;
		return true;
	}

	return false;
}

/*
==================
OGG_Update
==================
*/
void OGG_Update (void)
{
	stb_vorbis_info info;
	int samples_read;
	int ahead;
	float vol;
	int i, total_pcm;

	if (!ogg_playing || !ogg_stream || !ogg_enable || !ogg_enable->value)
		return;

	/* Don't overfill the DMA raw audio ring buffer */
	ahead = s_rawend - paintedtime;
	if (ahead > 4096)
		return;

	info = stb_vorbis_get_info (ogg_stream);
	if (info.channels <= 0 || info.channels > 2 || info.sample_rate <= 0)
	{
		OGG_Stop ();
		return;
	}

	samples_read = stb_vorbis_get_samples_short_interleaved (
		ogg_stream, info.channels, pcm_samples, OGG_CHUNK_SAMPLES * info.channels);

	if (samples_read > 0)
	{
		vol = ogg_volume ? ogg_volume->value : 1.0f;
		if (vol < 0.0f) vol = 0.0f;
		if (vol > 2.0f) vol = 2.0f;

		if (vol != 1.0f)
		{
			total_pcm = samples_read * info.channels;
			for (i = 0; i < total_pcm; i++)
			{
				int sample = (int)(pcm_samples[i] * vol);
				if (sample > 32767) sample = 32767;
				if (sample < -32768) sample = -32768;
				pcm_samples[i] = (short)sample;
			}
		}

		S_RawSamples (samples_read, info.sample_rate, 2, info.channels, (byte *)pcm_samples);
	}

	/* Check if track finished */
	if (samples_read < OGG_CHUNK_SAMPLES)
	{
		if (ogg_looping)
		{
			stb_vorbis_seek_start (ogg_stream);
		}
		else
		{
			OGG_Stop ();
		}
	}
}

/*
==================
OGG_Activate
==================
*/
void OGG_Activate (qboolean active)
{
	/* Mute or pause if deactivated when background audio is off */
}

/*
==================
OGG_Commands
==================
*/
static void OGG_Play_f (void)
{
	int track;

	if (Cmd_Argc () < 2)
	{
		Com_Printf ("Usage: ogg_play <track#> or ogg_play <file.ogg>\n");
		return;
	}

	track = atoi (Cmd_Argv (1));
	if (track > 0)
	{
		if (!OGG_Play (track, true))
		{
			Com_Printf ("OGG track %d not found in music/\n", track);
		}
	}
	else
	{
		if (!OGG_PlayFile (Cmd_Argv (1), true))
		{
			Com_Printf ("OGG file '%s' not found\n", Cmd_Argv (1));
		}
	}
}

static void OGG_Stop_f (void)
{
	OGG_Stop ();
}

static void OGG_Pause_f (void)
{
	OGG_Pause ();
}

/*
==================
OGG_Init
==================
*/
void OGG_Init (void)
{
	ogg_enable   = Cvar_Get ("ogg_enable", "1", CVAR_ARCHIVE);
	ogg_volume   = Cvar_Get ("ogg_volume", "0.7", CVAR_ARCHIVE);
	ogg_autoloop = Cvar_Get ("ogg_autoloop", "1", CVAR_ARCHIVE);

	Cmd_AddCommand ("ogg_play", OGG_Play_f);
	Cmd_AddCommand ("ogg_stop", OGG_Stop_f);
	Cmd_AddCommand ("ogg_pause", OGG_Pause_f);

	Com_Printf ("Ogg Vorbis soundtrack subsystem initialized.\n");
}

/*
==================
OGG_Shutdown
==================
*/
void OGG_Shutdown (void)
{
	OGG_Stop ();
}
