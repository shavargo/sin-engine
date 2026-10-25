/*
==============================================================================

Yamagi Quake II style Ogg Vorbis soundtrack streaming subsystem for SiN MP
Uses stb_vorbis for lightweight, dependency-free audio playback.

==============================================================================
*/

#ifndef CL_OGG_H
#define CL_OGG_H

#include "../game/q_shared.h"

void OGG_Init (void);
void OGG_Shutdown (void);
qboolean OGG_Play (int track, qboolean looping);
qboolean OGG_PlayFile (const char *filename, qboolean looping);
void OGG_Stop (void);
void OGG_Pause (void);
void OGG_Update (void);
void OGG_Activate (qboolean active);
qboolean OGG_IsPlaying (void);

#endif /* CL_OGG_H */
