/*
 * SiN game DLL API (Ritual SDK game.h, Steam MP bumped to version 14).
 * Layout verified vs GetGameAPI @ gamex86 0x1006fa90 (copies 0x5a import dwords).
 */
#ifndef SIN_GAME_H
#define SIN_GAME_H

#include "sin_game_types.h"

#define GAME_API_VERSION	14

#define FRAMETIME	0.1f

#define SVF_NOCLIENT		0x00000001
#define SVF_DEADMONSTER		0x00000002
#define SVF_MONSTER			0x00000004
#define SVF_SHOOTABLE		0x00000008
#define SVF_USEBBOX			0x00000010
#define SVF_ONLYPARENT		0x00000020
#define SVF_HIDEOWNER		0x00000040
#define SVF_MONSTERCLIP		0x00000080
#define SVF_PLAYERCLIP		0x00000100
#define SVF_NOTOWNER		0x00000200

typedef enum
{
	SOLID_NOT,
	SOLID_TRIGGER,
	SOLID_BBOX,
	SOLID_BSP
} solid_t;

typedef struct link_s
{
	struct link_s	*prev, *next;
} link_t;

#define MAX_ENT_CLUSTERS	16

typedef struct edict_s edict_t;
typedef struct gclient_s gclient_t;
typedef struct netconsole_s netconsole_t;
typedef struct netconbuffer_s netconbuffer_t;
typedef struct netsurface_s netsurface_t;

#ifndef GAME_INCLUDE

struct gclient_s
{
	player_state_t	ps;
	int				ping;
};

struct edict_s
{
	entity_state_t	s;
	struct gclient_s	*client;
	qboolean		inuse;
	int				linkcount;

	link_t			area;

	int				num_clusters;
	int				clusternums[MAX_ENT_CLUSTERS];
	int				headnode;
	int				areanum, areanum2;

	int				svflags;
	vec3_t			mins, maxs;
	vec3_t			absmin, absmax, size;
	vec3_t			fullmins, fullmaxs;
	float			fullradius;
	vec3_t			centroid;
	solid_t			solid;
	int				clipmask;
	edict_t			*owner;
};

#endif

struct netconsole_s
{
	console_state_t	s;
	qboolean		inuse;
};

struct netconbuffer_s
{
	console_buffer_state_t	s;
};

struct netsurface_s
{
	surface_state_t	s;
	qboolean		inuse;
};

typedef struct
{
	void	(*bprintf) (int printlevel, const char *fmt, ...);
	void	(*dprintf) (const char *fmt, ...);
	void	(*printf) (const char *fmt, ...);
	void	(*cprintf) (edict_t *ent, int printlevel, const char *fmt, ...);
	void	(*centerprintf) (edict_t *ent, const char *fmt, ...);
	void	(*sound) (edict_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs, float pitch, float fadetime, int flags);
	void	(*positioned_sound) (vec3_t origin, edict_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs, float pitch, float fadetime, int flags);

	void	(*configstring) (int num, const char *string);
	void	(*error) (const char *fmt, ...);

	int		(*modelindex) (const char *name);
	int		(*soundindex) (const char *name);
	int		(*imageindex) (const char *name);
	int		(*itemindex) (const char *name);

	void	(*setmodel) (edict_t *ent, const char *name);

	trace_t	(*trace) (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passent, int contentmask);
	trace_t	(*fulltrace) (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, float radius, edict_t *passent, int contentmask);

	int		(*pointcontents) (vec3_t point);
	qboolean	(*inPVS) (vec3_t p1, vec3_t p2);
	qboolean	(*inPHS) (vec3_t p1, vec3_t p2);
	void	(*SetAreaPortalState) (int portalnum, qboolean open);
	qboolean	(*AreasConnected) (int area1, int area2);

	void	(*linkentity) (edict_t *ent);
	void	(*unlinkentity) (edict_t *ent);
	int		(*BoxEdicts) (vec3_t mins, vec3_t maxs, edict_t **list, int maxcount, int areatype);
	void	(*Pmove) (pmove_t *pmove);

	void	(*multicast) (vec3_t origin, multicast_t to);
	void	(*unicast) (edict_t *ent, qboolean reliable);
	void	(*WriteChar) (int c);
	void	(*WriteByte) (int c);
	void	(*WriteShort) (int c);
	void	(*WriteLong) (int c);
	void	(*WriteFloat) (float f);
	void	(*WriteString) (const char *s);
	void	(*WritePosition) (vec3_t pos);
	void	(*WriteDir) (vec3_t pos);
	void	(*WriteAngle) (float f);

	void	*(*TagMalloc) (int size, int tag);
	void	(*TagFree) (void *block);
	void	(*FreeTags) (int tag);

	cvar_t	*(*cvar) (const char *var_name, const char *value, int flags);
	cvar_t	*(*cvar_set) (const char *var_name, const char *value);
	cvar_t	*(*cvar_forceset) (const char *var_name, const char *value);

	int		(*argc) (void);
	const char	*(*argv) (int n);
	const char	*(*args) (void);
	void	(*AddCommandString) (const char *text);
	void	(*DebugGraph) (float value, int color);

	int		(*LoadFile) (const char *path, void **buffer, int tag);
	const char	*(*GameDir) (void);
	const char	*(*PlayerDir) (void);
	void	(*CreatePath) (const char *path);
	float	(*SoundLength) (const char *path);

	qboolean	(*IsModel) (int index);

	int		(*NumAnims) (int modelindex);
	int		(*NumSkins) (int modelindex);
	int		(*NumGroups) (int modelindex);
	sinmdl_cmd_t *(*InitCommands) (int modelindex);
	void	(*CalculateBounds) (int modelindex, float scale, vec3_t mins, vec3_t maxs);

	const char *(*Anim_NameForNum) (int modelindex, int animnum);
	int		(*Anim_NumForName) (int modelindex, const char *name);
	int		(*Anim_Random) (int modelindex, const char *name);
	int		(*Anim_NumFrames) (int modelindex, int animnum);
	float	(*Anim_Time) (int modelindex, int animnum);
	void	(*Anim_Delta) (int modelindex, int animnum, vec3_t delta);

	sinmdl_cmd_t *(*Frame_Commands) (int modelindex, int animnum, int framenum);
	void	(*Frame_Delta) (int modelindex, int animnum, int framenum, vec3_t delta);
	float	(*Frame_Time) (int modelindex, int animnum, int framenum);

	const char *(*Skin_NameForNum) (int modelindex, int skinnum);
	int		(*Skin_NumForName) (int modelindex, const char *name);

	int		(*Group_NameToNum) (int modelindex, const char *name);
	const char *(*Group_NumToName) (int modelindex, int num);
	float	(*Group_DamageMultiplier) (int modelindex, int num);
	int		(*Group_Flags) (int modelindex, int num);

	qboolean	(*GetBoneInfo) (int modelindex, const char *bonename, int *groupindex, int *tri_num, vec3_t orientation);
	const char *(*GetBoneGroupName) (int modelindex, const char *bonename);
	qboolean	(*GetBoneTransform) (int modelindex, int groupindex, int tri_num, vec3_t orientation, int anim, int frame, float scale, vec3_t trans[3], vec3_t pos);

	qboolean	(*Alias_Add) (int modelindex, const char *alias, const char *name, float weight);
	const char *(*Alias_FindRandom) (int modelindex, const char *alias);
	void	(*Alias_Dump) (int modelindex);
	void	(*Alias_Clear) (int modelindex);

	qboolean	(*GlobalAlias_Add) (const char *alias, const char *name, float weight);
	const char *(*GlobalAlias_FindRandom) (const char *alias);
	void	(*GlobalAlias_Dump) (void);
	void	(*GlobalAlias_Clear) (void);

	unsigned short	(*CalcCRC) (const char *start, int count);

	int		(*Surf_NumSurfaces) (void);
	csurface_t *(*Surf_Surfaces) (void);

	void	(*IncrementStatusCount) (int i);
	debugline_t	**DebugLines;
	int		*numDebugLines;
} game_import_t;

typedef struct
{
	int			apiversion;

	void		(*Init) (void);
	void		(*Shutdown) (void);
	void		(*SpawnEntities) (const char *mapname, const char *entstring, const char *spawnpoint);

	void		(*WriteGame) (const char *filename, qboolean autosave);
	void		(*ReadGame) (const char *filename);
	void		(*WriteLevel) (const char *filename, qboolean autosave);
	void		(*ReadLevel) (const char *filename);

	qboolean	(*ClientConnect) (edict_t *ent, const char *userinfo);
	void		(*ClientBegin) (edict_t *ent, qboolean loadgame);
	void		(*ClientUserinfoChanged) (edict_t *ent, const char *userinfo);
	void		(*ClientDisconnect) (edict_t *ent);
	void		(*ClientCommand) (edict_t *ent);
	void		(*ClientThink) (edict_t *ent, usercmd_t *cmd);

	void		(*RunFrame) (void);
	void		(*ServerCommand) (void);

	void		(*CreateSurfaces) (csurface_t *surfaces, int count);

	struct edict_s	*edicts;
	int			edict_size;
	int			num_edicts;
	int			max_edicts;

	struct netconsole_s	*consoles;
	int			console_size;
	int			num_consoles;
	int			max_consoles;

	struct netconbuffer_s	*conbuffers;
	int			conbuffer_size;

	struct netsurface_s	*surfaces;
	int			surface_size;
	int			max_surfaces;
	int			num_surfaces;
} game_export_t;

game_export_t *GetGameAPI (game_import_t *import);

void Sin_FillGameImport (game_import_t *import);

#endif
