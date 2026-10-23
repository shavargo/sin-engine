/*
 * Bridge for SIN_STATIC_GAME: stock Quake 2 game objects (API 3) are
 * compiled -USIN_MP and expect game/game.h import layout. The rest of
 * sin_ded still builds with SIN_MP / sin_game.h, so SV_InitGameProgs
 * must not pass a SiN game_import_t into GetGameAPI.
 */
#include "../qcommon/qcommon.h"
#include "../game/game.h"

/* Engine helpers live in sv_game.c / server (edict_t* is pointer-compatible). */
void	PF_Unicast (struct edict_s *ent, qboolean reliable);
void	PF_dprintf (char *fmt, ...);
void	PF_cprintf (struct edict_s *ent, int level, char *fmt, ...);
void	PF_centerprintf (struct edict_s *ent, char *fmt, ...);
void	PF_error (char *fmt, ...);
void	PF_setmodel (struct edict_s *ent, char *name);
void	PF_Configstring (int index, char *val);
void	PF_WriteChar (int c);
void	PF_WriteByte (int c);
void	PF_WriteShort (int c);
void	PF_WriteLong (int c);
void	PF_WriteFloat (float f);
void	PF_WriteString (char *s);
void	PF_WritePos (vec3_t pos);
void	PF_WriteDir (vec3_t dir);
void	PF_WriteAngle (float f);
qboolean	PF_inPVS (vec3_t p1, vec3_t p2);
qboolean	PF_inPHS (vec3_t p1, vec3_t p2);
void	PF_StartSound (struct edict_s *entity, int channel, int sound_num,
	float volume, float attenuation, float timeofs);

void	SV_Multicast (vec3_t origin, multicast_t to);
void	SV_BroadcastPrintf (int level, char *fmt, ...);
void	SV_LinkEdict (struct edict_s *ent);
void	SV_UnlinkEdict (struct edict_s *ent);
int		SV_AreaEdicts (vec3_t mins, vec3_t maxs, struct edict_s **list,
	int maxcount, int areatype);
trace_t	SV_Trace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end,
	struct edict_s *passent, int contentmask);
int		SV_PointContents (vec3_t p);
int		SV_ModelIndex (char *name);
int		SV_SoundIndex (char *name);
int		SV_ImageIndex (char *name);
void	SV_StartSound (vec3_t origin, struct edict_s *entity, int channel,
	int soundindex, float volume, float attenuation, float timeofs);

void	SCR_DebugGraph (float value, int color);

game_export_t *GetGameAPI (game_import_t *import);

/*
===============
SV_InitStaticQ2Game

Fill a Q2 game_import_t and return the hard-linked GetGameAPI export.
===============
*/
void *SV_InitStaticQ2Game (void)
{
	game_import_t	import;

	memset (&import, 0, sizeof (import));

	import.multicast = SV_Multicast;
	import.unicast = PF_Unicast;
	import.bprintf = SV_BroadcastPrintf;
	import.dprintf = PF_dprintf;
	import.cprintf = PF_cprintf;
	import.centerprintf = PF_centerprintf;
	import.error = PF_error;

	import.linkentity = SV_LinkEdict;
	import.unlinkentity = SV_UnlinkEdict;
	import.BoxEdicts = SV_AreaEdicts;
	import.trace = SV_Trace;
	import.pointcontents = SV_PointContents;
	import.setmodel = PF_setmodel;
	import.inPVS = PF_inPVS;
	import.inPHS = PF_inPHS;
	import.Pmove = Pmove;

	import.modelindex = SV_ModelIndex;
	import.soundindex = SV_SoundIndex;
	import.imageindex = SV_ImageIndex;

	import.configstring = PF_Configstring;
	import.sound = PF_StartSound;
	import.positioned_sound = SV_StartSound;

	import.WriteChar = PF_WriteChar;
	import.WriteByte = PF_WriteByte;
	import.WriteShort = PF_WriteShort;
	import.WriteLong = PF_WriteLong;
	import.WriteFloat = PF_WriteFloat;
	import.WriteString = PF_WriteString;
	import.WritePosition = PF_WritePos;
	import.WriteDir = PF_WriteDir;
	import.WriteAngle = PF_WriteAngle;

	import.TagMalloc = Z_TagMalloc;
	import.TagFree = Z_Free;
	import.FreeTags = Z_FreeTags;

	import.cvar = Cvar_Get;
	import.cvar_set = Cvar_Set;
	import.cvar_forceset = Cvar_ForceSet;

	import.argc = Cmd_Argc;
	import.argv = Cmd_Argv;
	import.args = Cmd_Args;
	import.AddCommandString = Cbuf_AddText;

	import.DebugGraph = SCR_DebugGraph;
	import.SetAreaPortalState = CM_SetAreaPortalState;
	import.AreasConnected = CM_AreasConnected;

	return GetGameAPI (&import);
}
