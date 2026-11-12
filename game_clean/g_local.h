/*
 * Clean-room GPL implementation of SiN Game Module (API 14)
 * For use with sin-engine.
 */
#ifndef G_LOCAL_H
#define G_LOCAL_H

#include "../sin/sin_game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define svc_temp_entity 3

#define MAX_EDICTS 1024
#define MAX_SPAWN_POINTS 128
#define MAX_DOORS 64

typedef struct
{
	vec3_t	origin;
	vec3_t	angles;
	char	classname[64];
} spawnpoint_t;

typedef enum
{
	DOOR_CLOSED = 0,
	DOOR_OPENING,
	DOOR_OPEN,
	DOOR_CLOSING
} door_state_t;

typedef struct
{
	edict_t			*ent;
	vec3_t			pos_closed;
	vec3_t			pos_open;
	vec3_t			dir;
	float			distance;
	float			speed;
	float			wait;
	float			wait_timer;
	door_state_t	state;
	int				sound_move;
	int				sound_stop;
} door_t;

typedef struct
{
	player_state_t	ps;
	int				ping;

	vec3_t			velocity;
	float			viewheight;

	edict_t			*gun_ent;
	int				current_weapon;	/* 0 = fists, 1 = magnum, 2 = shotgun, 3 = assault rifle */
	int				weapon_fire_wait;
} gclient_local_t;

typedef struct
{
	game_import_t	gi;

	edict_t			*edicts;
	gclient_t		*clients;
	gclient_local_t	client_locals[32];
	int				num_edicts;
	int				max_edicts;
	int				max_clients;

	spawnpoint_t	spawns[MAX_SPAWN_POINTS];
	int				num_spawns;

	door_t			doors[MAX_DOORS];
	int				num_doors;

	float			frametime;
	float			time;

	int				model_view_punch;
	int				model_view_magnum;
	int				model_view_shotgun;
	int				model_view_asrifle;

	int				sound_magnum_fire;
	int				sound_shotgun_fire;
	int				sound_punch_swing;
	int				sound_asrifle_fire;
} game_locals_t;

extern game_locals_t game;
extern game_import_t gi;

static inline void G_Sound (edict_t *ent, int channel, int soundindex, float volume, float attenuation)
{
	gi.sound (ent, channel, soundindex, volume, attenuation, 0.0f, 1.0f, 0.0f, 0);
}

/* Edict allocation & lookup */
void G_InitEdicts (void);
edict_t *G_AllocEdict (void);
void G_FreeEdict (edict_t *ed);

/* Spawn system & doors */
void G_Precache (void);
void SpawnEntities (const char *mapname, const char *entstring, const char *spawnpoint);
qboolean SelectSpawnPoint (vec3_t origin, vec3_t angles);
void G_UpdateDoors (float frametime);

/* Client handling */
qboolean ClientConnect (edict_t *ent, const char *userinfo);
void ClientBegin (edict_t *ent, qboolean loadgame);
void ClientUserinfoChanged (edict_t *ent, const char *userinfo);
void ClientDisconnect (edict_t *ent);
void ClientCommand (edict_t *ent);
void ClientThink (edict_t *ent, usercmd_t *cmd);

#endif /* G_LOCAL_H */
