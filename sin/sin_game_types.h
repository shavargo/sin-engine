/* Minimal SiN SDK types needed by sin_game.h (API 14 import/export). */
#ifndef SIN_GAME_TYPES_H
#define SIN_GAME_TYPES_H

#include "../game/q_shared.h"

/* bone_t / MAX_MODEL_GROUPS live in q_shared.h under SIN_MP */
#define SINMDL_CMD_MAX_CMDS		128
#define SINMDL_CMD_MAX_ARGS		32
#define MAX_CONSOLE_NAME		32
#define MAX_LAYOUT_LENGTH		1024
#define MAX_BUFFER_LENGTH		4096
#define MAX_STATUSBARS			8
#define MAXCMDLINE				256

typedef struct
{
	int		num_args;
	char	*args[SINMDL_CMD_MAX_ARGS];
} sinmdl_singlecmd_t;

typedef struct
{
	int					num_cmds;
	sinmdl_singlecmd_t	cmds[SINMDL_CMD_MAX_CMDS];
} sinmdl_cmd_t;

typedef struct
{
	vec3_t	start;
	vec3_t	end;
	vec3_t	color;
	float	alpha;
} debugline_t;

/* Opaque-ish stubs — sizes match Steam game DLL expectations for pointers only */
typedef struct
{
	int		number;
	char	console_name[MAX_CONSOLE_NAME];
	byte	pad[512];
} console_state_t;

typedef struct
{
	int		start_index, end_index, start, end;
	char	buffer[MAX_BUFFER_LENGTH];
} console_buffer_state_t;

typedef struct
{
	qboolean	changed;
	char		*name;
	int			number;
	int			groupnumber;
	byte		pad[64];
} surface_state_t;

#endif
