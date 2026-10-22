/*
 * Fill SiN game_import_t (API 14) for Steam base/gamex86.dll.
 * SINMDL .def/.sam bounds feed IsModel / CalculateBounds / Anim_*.
 */
#include "../qcommon/qcommon.h"
#include "../server/server.h"
#include "sin_game.h"
#include "sinmdl.h"

/* declared in sv_game.c */
void	PF_Unicast (edict_t *ent, qboolean reliable);
void	PF_dprintf (char *fmt, ...);
void	PF_cprintf (edict_t *ent, int level, char *fmt, ...);
void	PF_centerprintf (edict_t *ent, char *fmt, ...);
void	PF_error (char *fmt, ...);
void	PF_setmodel (edict_t *ent, char *name);
qboolean PF_inPVS (vec3_t p1, vec3_t p2);
qboolean PF_inPHS (vec3_t p1, vec3_t p2);
void	PF_Configstring (int index, char *string);
void	PF_WriteChar (int c);
void	PF_WriteByte (int c);
void	PF_WriteShort (int c);
void	PF_WriteLong (int c);
void	PF_WriteFloat (float f);
void	PF_WriteString (char *s);
void	PF_WritePos (vec3_t pos);
void	PF_WriteDir (vec3_t dir);
void	PF_WriteAngle (float f);
void	PF_StartSound (edict_t *entity, int channel, int sound_num, float volume,
	float attenuation, float timeofs);
void	SCR_DebugGraph (float value, int color);
#ifdef SIN_MP
void	Sys_SinCrashCrumb (const char *msg);
#else
#define Sys_SinCrashCrumb(m)	((void)0)
#endif

static debugline_t	*sin_debug_lines;
static int			sin_num_debug_lines;

/* Empty command list — game iterates without null-checking Frame/InitCommands. */
static sinmdl_cmd_t	sin_empty_cmds;

#define SIN_ALIAS_MAX		2048
#define SIN_ALIAS_NAME_LEN	64

typedef struct
{
	int		modelindex;	/* -1 = global */
	char	alias[SIN_ALIAS_NAME_LEN];
	char	name[MAX_QPATH];
	float	weight;
	qboolean	used;
} sin_alias_t;

static sin_alias_t	sin_aliases[SIN_ALIAS_MAX];

static void Sin_bprintf (int level, const char *fmt, ...)
{
	va_list	argptr;
	char	msg[1024];

	va_start (argptr, fmt);
	vsprintf (msg, fmt, argptr);
	va_end (argptr);
	Sys_SinCrashCrumb ("gi.bprintf");
	SV_BroadcastPrintf (level, "%s", msg);
	Sys_SinCrashCrumb ("gi.bprintf.done");
}

static void Sin_dprintf (const char *fmt, ...)
{
	va_list	argptr;
	char	msg[1024];

	va_start (argptr, fmt);
	vsprintf (msg, fmt, argptr);
	va_end (argptr);
	PF_dprintf ("%s", msg);
}

static void Sin_printf (const char *fmt, ...)
{
	va_list	argptr;
	char	msg[1024];

	va_start (argptr, fmt);
	vsprintf (msg, fmt, argptr);
	va_end (argptr);
	Com_Printf ("%s", msg);
}

static void Sin_cprintf (edict_t *ent, int level, const char *fmt, ...)
{
	va_list	argptr;
	char	msg[1024];

	va_start (argptr, fmt);
	vsprintf (msg, fmt, argptr);
	va_end (argptr);
	PF_cprintf (ent, level, "%s", msg);
}

static void Sin_centerprintf (edict_t *ent, const char *fmt, ...)
{
	va_list	argptr;
	char	msg[1024];

	va_start (argptr, fmt);
	vsprintf (msg, fmt, argptr);
	va_end (argptr);
	PF_centerprintf (ent, "%s", msg);
}

static void Sin_error (const char *fmt, ...)
{
	va_list	argptr;
	char	msg[1024];

	va_start (argptr, fmt);
	vsprintf (msg, fmt, argptr);
	va_end (argptr);
	PF_error ("%s", msg);
}

static void Sin_sound (edict_t *ent, int channel, int soundindex, float volume,
	float attenuation, float timeofs, float pitch, float fadetime, int flags)
{
	(void)pitch; (void)fadetime; (void)flags;
	if (!ent || soundindex < 1)
		return;
	if (volume < 0.f || volume > 1.f)
		volume = 1.f;
	if (attenuation < 0.f || attenuation > 4.f)
		attenuation = 1.f;
	if (timeofs < 0.f || timeofs > 0.255f)
		timeofs = 0.f;
	PF_StartSound (ent, channel, soundindex, volume, attenuation, timeofs);
}

static void Sin_positioned_sound (vec3_t origin, edict_t *ent, int channel, int soundindex,
	float volume, float attenuation, float timeofs, float pitch, float fadetime, int flags)
{
	(void)pitch; (void)fadetime; (void)flags;
	SV_StartSound (origin, ent, channel, soundindex, volume, attenuation, timeofs);
}

static void Sin_configstring (int num, const char *string)
{
	PF_Configstring (num, (char *)string);
}

static int Sin_modelindex (const char *name)
{
	int	i;

	i = SV_ModelIndex ((char *)name);
	if (i > 0 && name)
		SINMDL_RegisterDefForIndex (i, name);
	return i;
}
static int Sin_soundindex (const char *name) { return SV_SoundIndex ((char *)name); }
static int Sin_imageindex (const char *name) { return SV_ImageIndex ((char *)name); }
static int Sin_itemindex (const char *name) { (void)name; return 0; }

static void Sin_setmodel (edict_t *ent, const char *name)
{
	int	i;

	Sys_SinCrashCrumb ("gi.setmodel");
	PF_setmodel (ent, (char *)name);
	if (!ent || !name)
		return;
	i = ent->s.modelindex;
	if (i > 0)
		SINMDL_RegisterDefForIndex (i, name);
	Sys_SinCrashCrumb ("gi.setmodel.done");
}

static trace_t Sin_fulltrace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end,
	float radius, edict_t *passent, int contentmask)
{
	(void)radius;
	return SV_Trace (start, mins, maxs, end, passent, contentmask);
}

static void Sin_WriteString (const char *s)
{
	PF_WriteString ((char *)s);
}

static cvar_t *Sin_cvar (const char *n, const char *v, int f)
{
	return Cvar_Get ((char *)n, (char *)v, f);
}
static cvar_t *Sin_cvar_set (const char *n, const char *v)
{
	return Cvar_Set ((char *)n, (char *)v);
}
static cvar_t *Sin_cvar_forceset (const char *n, const char *v)
{
	return Cvar_ForceSet ((char *)n, (char *)v);
}

static const char *Sin_argv (int n) { return Cmd_Argv (n); }
static const char *Sin_args (void) { return Cmd_Args (); }
static void Sin_AddCommandString (const char *text) { Cbuf_AddText ((char *)text); }

static int Sin_LoadFile (const char *path, void **buffer, int tag)
{
	int		len;
	byte	*buf;

	(void)tag;
	len = FS_LoadFile ((char *)path, (void **)&buf);
	if (len < 0)
	{
		if (buffer)
			*buffer = NULL;
		return -1;
	}
	if (buffer)
		*buffer = buf;
	else
		FS_FreeFile (buf);
	return len;
}

static const char *Sin_GameDir (void) { return FS_Gamedir (); }
static const char *Sin_PlayerDir (void) { return FS_Gamedir (); }

static void Sin_CreatePath (const char *path)
{
	(void)path; /* FS_CreatePath not always exported; ignore for smoke */
}

static float Sin_SoundLength (const char *path) { (void)path; return 0.f; }

static qboolean Sin_IsModel (int index)
{
	return SINMDL_IsModel (index);
}

static const char *Sin_Skin_NameForNum (int m, int s) { (void)m; (void)s; return ""; }
static int Sin_Skin_NumForName (int m, const char *n) { (void)m; (void)n; return 0; }
static int Sin_Group_NameToNum (int m, const char *n)
{
	return SINMDL_Group_NameToNum (m, n);
}
static const char *Sin_Group_NumToName (int m, int n)
{
	return SINMDL_Group_NumToName (m, n);
}
static float Sin_Group_DamageMultiplier (int m, int n) { (void)m; (void)n; return 1.f; }
static int Sin_Group_Flags (int m, int n) { (void)m; (void)n; return 0; }
static sinmdl_cmd_t *Sin_Frame_Commands (int a, int b, int c)
{
	return SINMDL_Frame_Commands (a, b, c);
}

static qboolean Sin_alias_add (int modelindex, const char *alias, const char *name, float weight)
{
	int	i;
	int	free_slot = -1;

	Sys_SinCrashCrumb ("gi.Alias_Add");
	if (!alias || !alias[0] || !name || !name[0])
		return false;

	for (i = 0; i < SIN_ALIAS_MAX; i++)
	{
		if (!sin_aliases[i].used)
		{
			if (free_slot < 0)
				free_slot = i;
			continue;
		}
		if (sin_aliases[i].modelindex == modelindex
			&& !Q_strcasecmp (sin_aliases[i].alias, (char *)alias)
			&& !Q_strcasecmp (sin_aliases[i].name, (char *)name))
		{
			sin_aliases[i].weight = weight;
			return true;
		}
	}
	if (free_slot < 0)
		return false;

	sin_aliases[free_slot].used = true;
	sin_aliases[free_slot].modelindex = modelindex;
	sin_aliases[free_slot].weight = weight;
	strncpy (sin_aliases[free_slot].alias, alias, sizeof (sin_aliases[free_slot].alias) - 1);
	sin_aliases[free_slot].alias[sizeof (sin_aliases[free_slot].alias) - 1] = 0;
	strncpy (sin_aliases[free_slot].name, name, sizeof (sin_aliases[free_slot].name) - 1);
	sin_aliases[free_slot].name[sizeof (sin_aliases[free_slot].name) - 1] = 0;
	return true;
}

/*
 * Steam Alias_FindRandom returns a heap string the game frees (see
 * player::sound free(param_2)). Never return static/alias-table pointers.
 */
/* Game free()s Alias_FindRandom results (not TagFree) — use CRT malloc. */
static const char *Sin_alias_strdup (const char *s)
{
	char	*p;
	size_t	n;

	if (!s)
		s = "";
	n = strlen (s) + 1;
	p = (char *)malloc (n);
	if (!p)
		return "";
	memcpy (p, s, n);
	return p;
}

static const char *Sin_alias_find (int modelindex, const char *alias)
{
	int		i;
	float	total = 0.f;
	float	r;
	int		nmatch = 0;
	int		matches[64];
	const char	*picked;

	Sys_SinCrashCrumb ("gi.Alias_FindRandom");
	if (!alias || !alias[0])
	{
		Sys_SinCrashCrumb ("gi.Alias_FindRandom.empty");
		return Sin_alias_strdup ("");
	}

	for (i = 0; i < SIN_ALIAS_MAX && nmatch < 64; i++)
	{
		if (!sin_aliases[i].used)
			continue;
		if (sin_aliases[i].modelindex != modelindex)
			continue;
		if (Q_strcasecmp (sin_aliases[i].alias, (char *)alias))
			continue;
		matches[nmatch++] = i;
		total += (sin_aliases[i].weight > 0.f) ? sin_aliases[i].weight : 1.f;
	}
	if (!nmatch)
	{
		/* Miss: still heap-copy the token so free() is safe. */
		Sys_SinCrashCrumb ("gi.Alias_FindRandom.miss");
		return Sin_alias_strdup (alias);
	}
	if (nmatch == 1 || total <= 0.f)
		picked = sin_aliases[matches[0]].name;
	else
	{
		r = (float)(rand () % 1000) / 1000.f * total;
		picked = sin_aliases[matches[nmatch - 1]].name;
		for (i = 0; i < nmatch; i++)
		{
			float	w = (sin_aliases[matches[i]].weight > 0.f)
				? sin_aliases[matches[i]].weight : 1.f;
			if (r <= w)
			{
				picked = sin_aliases[matches[i]].name;
				break;
			}
			r -= w;
		}
	}
	Sys_SinCrashCrumb ("gi.Alias_FindRandom.hit");
	return Sin_alias_strdup (picked);
}

static void Sin_alias_clear (int modelindex)
{
	int	i;

	for (i = 0; i < SIN_ALIAS_MAX; i++)
	{
		if (sin_aliases[i].used && sin_aliases[i].modelindex == modelindex)
			sin_aliases[i].used = false;
	}
}

static void Sin_FrameDelta (int modelindex, int animnum, int framenum, vec3_t delta)
{
	SINMDL_Frame_Delta (modelindex, animnum, framenum, delta);
}

static float Sin_FrameTime (int a, int b, int c)
{
	return SINMDL_Frame_Time (a, b, c);
}

static qboolean Sin_GetBoneInfo (int modelindex, const char *bonename, int *groupindex, int *tri_num, vec3_t orientation)
{
	return SINMDL_GetBoneInfo (modelindex, bonename, groupindex, tri_num, orientation);
}

static const char *Sin_GetBoneGroupName (int modelindex, const char *bonename)
{
	return SINMDL_GetBoneGroupName (modelindex, bonename);
}

static qboolean Sin_GetBoneTransform (int modelindex, int groupindex, int tri_num, vec3_t orientation,
	int anim, int frame, float scale, vec3_t trans[3], vec3_t pos)
{
	return SINMDL_GetBoneTransform (modelindex, groupindex, tri_num, orientation,
		anim, frame, scale, trans, pos);
}

static qboolean Sin_Alias_Add (int m, const char *a, const char *n, float w)
{
	return Sin_alias_add (m, a, n, w);
}
static const char *Sin_Alias_FindRandom (int m, const char *a)
{
	return Sin_alias_find (m, a);
}
static void Sin_Alias_Dump (int m) { (void)m; }
static void Sin_Alias_Clear (int m) { Sin_alias_clear (m); }

static qboolean Sin_GlobalAlias_Add (const char *a, const char *n, float w)
{
	return Sin_alias_add (-1, a, n, w);
}
static const char *Sin_GlobalAlias_FindRandom (const char *a)
{
	return Sin_alias_find (-1, a);
}
static void Sin_GlobalAlias_Dump (void) {}
static void Sin_GlobalAlias_Clear (void) { Sin_alias_clear (-1); }

static unsigned short Sin_CalcCRC (const char *start, int count)
{
	return (unsigned short)Com_BlockChecksum ((void *)start, count);
}

static int Sin_Surf_NumSurfaces (void) { return 0; }
static csurface_t *Sin_Surf_Surfaces (void) { return NULL; }
static void Sin_IncrementStatusCount (int i) { (void)i; }

void Sin_FillGameImport (game_import_t *import)
{
	memset (import, 0, sizeof (*import));

	import->bprintf = Sin_bprintf;
	import->dprintf = Sin_dprintf;
	import->printf = Sin_printf;
	import->cprintf = Sin_cprintf;
	import->centerprintf = Sin_centerprintf;
	import->sound = Sin_sound;
	import->positioned_sound = Sin_positioned_sound;
	import->configstring = Sin_configstring;
	import->error = Sin_error;

	import->modelindex = Sin_modelindex;
	import->soundindex = Sin_soundindex;
	import->imageindex = Sin_imageindex;
	import->itemindex = Sin_itemindex;
	import->setmodel = Sin_setmodel;

	import->trace = SV_Trace;
	import->fulltrace = Sin_fulltrace;
	import->pointcontents = SV_PointContents;
	import->inPVS = PF_inPVS;
	import->inPHS = PF_inPHS;
	import->SetAreaPortalState = CM_SetAreaPortalState;
	import->AreasConnected = CM_AreasConnected;

	import->linkentity = SV_LinkEdict;
	import->unlinkentity = SV_UnlinkEdict;
	import->BoxEdicts = SV_AreaEdicts;
	import->Pmove = Pmove;

	import->multicast = SV_Multicast;
	import->unicast = PF_Unicast;
	import->WriteChar = PF_WriteChar;
	import->WriteByte = PF_WriteByte;
	import->WriteShort = PF_WriteShort;
	import->WriteLong = PF_WriteLong;
	import->WriteFloat = PF_WriteFloat;
	import->WriteString = Sin_WriteString;
	import->WritePosition = PF_WritePos;
	import->WriteDir = PF_WriteDir;
	import->WriteAngle = PF_WriteAngle;

	import->TagMalloc = Z_TagMalloc;
	import->TagFree = Z_Free;
	import->FreeTags = Z_FreeTags;

	import->cvar = Sin_cvar;
	import->cvar_set = Sin_cvar_set;
	import->cvar_forceset = Sin_cvar_forceset;

	import->argc = Cmd_Argc;
	import->argv = Sin_argv;
	import->args = Sin_args;
	import->AddCommandString = Sin_AddCommandString;
	import->DebugGraph = SCR_DebugGraph;

	import->LoadFile = Sin_LoadFile;
	import->GameDir = Sin_GameDir;
	import->PlayerDir = Sin_PlayerDir;
	import->CreatePath = Sin_CreatePath;
	import->SoundLength = Sin_SoundLength;

	import->IsModel = Sin_IsModel;
	import->NumAnims = SINMDL_NumAnims;
	import->NumSkins = SINMDL_NumSkins;
	import->NumGroups = SINMDL_NumGroups;
	import->InitCommands = SINMDL_InitCommands;
	import->CalculateBounds = SINMDL_CalculateBounds;

	import->Anim_NameForNum = SINMDL_Anim_NameForNum;
	import->Anim_NumForName = SINMDL_Anim_NumForName;
	import->Anim_Random = SINMDL_Anim_Random;
	import->Anim_NumFrames = SINMDL_Anim_NumFrames;
	import->Anim_Time = SINMDL_Anim_Time;
	import->Anim_Delta = SINMDL_Anim_Delta;

	import->Frame_Commands = Sin_Frame_Commands;
	import->Frame_Delta = Sin_FrameDelta;
	import->Frame_Time = Sin_FrameTime;

	import->Skin_NameForNum = Sin_Skin_NameForNum;
	import->Skin_NumForName = Sin_Skin_NumForName;

	import->Group_NameToNum = Sin_Group_NameToNum;
	import->Group_NumToName = Sin_Group_NumToName;
	import->Group_DamageMultiplier = Sin_Group_DamageMultiplier;
	import->Group_Flags = Sin_Group_Flags;

	import->GetBoneInfo = Sin_GetBoneInfo;
	import->GetBoneGroupName = Sin_GetBoneGroupName;
	import->GetBoneTransform = Sin_GetBoneTransform;

	import->Alias_Add = Sin_Alias_Add;
	import->Alias_FindRandom = Sin_Alias_FindRandom;
	import->Alias_Dump = Sin_Alias_Dump;
	import->Alias_Clear = Sin_Alias_Clear;

	import->GlobalAlias_Add = Sin_GlobalAlias_Add;
	import->GlobalAlias_FindRandom = Sin_GlobalAlias_FindRandom;
	import->GlobalAlias_Dump = Sin_GlobalAlias_Dump;
	import->GlobalAlias_Clear = Sin_GlobalAlias_Clear;

	import->CalcCRC = Sin_CalcCRC;
	import->Surf_NumSurfaces = Sin_Surf_NumSurfaces;
	import->Surf_Surfaces = Sin_Surf_Surfaces;
	import->IncrementStatusCount = Sin_IncrementStatusCount;
	import->DebugLines = &sin_debug_lines;
	import->numDebugLines = &sin_num_debug_lines;
}
