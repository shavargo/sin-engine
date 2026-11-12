/*
 * Clean-room GPL implementation of SiN Game Module (API 14)
 * Map entity string parsing, spawning, and door mechanics.
 */
#include "g_local.h"

static const char *ED_ParseToken (const char **data_p, char *token, int maxlen)
{
	const char *data = *data_p;
	int len = 0;

	token[0] = 0;
	if (!data)
		return NULL;

	/* Skip whitespace */
	while (*data && (*data <= ' ' || *data == '\r' || *data == '\n'))
		data++;

	if (!*data)
	{
		*data_p = data;
		return NULL;
	}

	/* Quoted string */
	if (*data == '"')
	{
		data++;
		while (*data && *data != '"' && len < maxlen - 1)
		{
			token[len++] = *data++;
		}
		if (*data == '"')
			data++;
		token[len] = 0;
		*data_p = data;
		return token;
	}

	/* Single char tokens */
	if (*data == '{' || *data == '}')
	{
		token[0] = *data++;
		token[1] = 0;
		*data_p = data;
		return token;
	}

	/* Unquoted token */
	while (*data && *data > ' ' && *data != '{' && *data != '}' && *data != '"' && len < maxlen - 1)
	{
		token[len++] = *data++;
	}
	token[len] = 0;
	*data_p = data;
	return token;
}

qboolean SelectSpawnPoint (vec3_t origin, vec3_t angles)
{
	int i;

	/* Look for info_player_deathmatch first */
	for (i = 0; i < game.num_spawns; i++)
	{
		if (strstr (game.spawns[i].classname, "deathmatch"))
		{
			VectorCopy (game.spawns[i].origin, origin);
			VectorCopy (game.spawns[i].angles, angles);
			return true;
		}
	}

	/* Fall back to info_player_start */
	for (i = 0; i < game.num_spawns; i++)
	{
		if (strstr (game.spawns[i].classname, "start"))
		{
			VectorCopy (game.spawns[i].origin, origin);
			VectorCopy (game.spawns[i].angles, angles);
			return true;
		}
	}

	/* Fallback if any spawn exists */
	if (game.num_spawns > 0)
	{
		VectorCopy (game.spawns[0].origin, origin);
		VectorCopy (game.spawns[0].angles, angles);
		return true;
	}

	VectorClear (origin);
	VectorClear (angles);
	return false;
}

void SpawnEntities (const char *mapname, const char *entstring, const char *spawnpoint)
{
	const char *data = entstring;
	char token[256];
	char key[64], value[256];
	char classname[64];
	char model[128];
	char sound_move[128], sound_stop[128];
	vec3_t origin, angles;
	float door_angle = 0.0f;
	qboolean has_angle = false;
	int count = 0;

	(void)mapname;
	(void)spawnpoint;

	G_InitEdicts ();
	G_Precache ();
	game.num_spawns = 0;
	game.num_doors = 0;

	while (data && *data)
	{
		if (!ED_ParseToken (&data, token, sizeof(token)))
			break;

		if (token[0] != '{')
			continue;

		classname[0] = 0;
		model[0] = 0;
		sound_move[0] = 0;
		sound_stop[0] = 0;
		door_angle = 0.0f;
		has_angle = false;
		VectorClear (origin);
		VectorClear (angles);

		while (data && *data)
		{
			if (!ED_ParseToken (&data, key, sizeof(key)))
				break;
			if (key[0] == '}')
				break;
			if (!ED_ParseToken (&data, value, sizeof(value)))
				break;

			if (!strcmp (key, "classname"))
				strncpy (classname, value, sizeof(classname) - 1);
			else if (!strcmp (key, "origin"))
				sscanf (value, "%f %f %f", &origin[0], &origin[1], &origin[2]);
			else if (!strcmp (key, "angle"))
			{
				door_angle = (float)atof (value);
				has_angle = true;
				angles[1] = door_angle;
			}
			else if (!strcmp (key, "angles"))
			{
				sscanf (value, "%f %f %f", &angles[0], &angles[1], &angles[2]);
				door_angle = angles[1];
				has_angle = true;
			}
			else if (!strcmp (key, "model"))
				strncpy (model, value, sizeof(model) - 1);
			else if (!strcmp (key, "sound_move"))
				strncpy (sound_move, value, sizeof(sound_move) - 1);
			else if (!strcmp (key, "sound_stop"))
				strncpy (sound_stop, value, sizeof(sound_stop) - 1);
		}

		if (!classname[0])
			continue;

		count++;

		/* Handle worldspawn */
		if (!strcmp (classname, "worldspawn"))
		{
			edict_t *world = &game.edicts[0];
			world->s.number = 0;
			world->s.modelindex = 1;
			world->solid = SOLID_BSP;
			world->inuse = true;
			continue;
		}

		/* Record player spawn locations */
		if (strstr (classname, "info_player_") || strstr (classname, "info_coop_"))
		{
			if (game.num_spawns < MAX_SPAWN_POINTS)
			{
				VectorCopy (origin, game.spawns[game.num_spawns].origin);
				VectorCopy (angles, game.spawns[game.num_spawns].angles);
				strncpy (game.spawns[game.num_spawns].classname, classname, 63);
				game.num_spawns++;
			}
			continue;
		}

		/* Spawn in-world brush models (e.g. *1, *2 etc for world geometry/doors/glass) */
		if (model[0] == '*')
		{
			edict_t *ent = G_AllocEdict ();
			if (ent)
			{
				ent->s.modelindex = gi.modelindex (model);
				VectorCopy (origin, ent->s.origin);
				/* Crucial: brush geometry is pre-oriented in local/world coords;
				 * setting s.angles would rotate the model visually! */
				VectorClear (ent->s.angles);

				if (!strncmp (classname, "trigger_", 8) || !strcmp (classname, "misc_teleporter"))
				{
					/* Triggers must never block player movement */
					ent->solid = SOLID_NOT;
				}
				else
				{
					ent->solid = SOLID_BSP;
				}

				gi.setmodel (ent, model);
				gi.linkentity (ent);

				/* Register doors for automatic opening/closing */
				if (!strcmp (classname, "func_door") && game.num_doors < MAX_DOORS)
				{
					door_t *door = &game.doors[game.num_doors++];
					memset (door, 0, sizeof(*door));
					door->ent = ent;
					VectorCopy (origin, door->pos_closed);
					door->speed = 100.0f;
					door->wait = 3.0f;
					door->state = DOOR_CLOSED;

					if (sound_move[0])
						door->sound_move = gi.soundindex (sound_move);
					else
						door->sound_move = gi.soundindex ("environment/Doors/door1opn.wav");

					if (sound_stop[0])
						door->sound_stop = gi.soundindex (sound_stop);
					else
						door->sound_stop = gi.soundindex ("environment/SWITCH/Sw1.wav");

					/* Compute sliding direction from door angle */
					if (has_angle && (int)door_angle == -1)
					{
						VectorSet (door->dir, 0, 0, 1);
					}
					else if (has_angle && (int)door_angle == -2)
					{
						VectorSet (door->dir, 0, 0, -1);
					}
					else
					{
						float rad = door_angle * (float)M_PI / 180.0f;
						door->dir[0] = cosf (rad);
						door->dir[1] = sinf (rad);
						door->dir[2] = 0.0f;
					}

					/* Slide open by 60 units along opening direction */
					door->distance = 60.0f;
					VectorMA (door->pos_closed, door->distance, door->dir, door->pos_open);
				}
			}
		}
	}

	gi.dprintf ("SpawnEntities: spawned %d entities (%d spawns, %d doors)\n",
		count, game.num_spawns, game.num_doors);
}

void G_UpdateDoors (float frametime)
{
	int i, p;

	for (i = 0; i < game.num_doors; i++)
	{
		door_t *door = &game.doors[i];
		edict_t *ent = door->ent;
		qboolean player_near = false;

		if (!ent || !ent->inuse)
			continue;

		/* Check distance to all active players */
		for (p = 0; p < game.max_clients; p++)
		{
			edict_t *ply = &game.edicts[1 + p];
			vec3_t diff;
			float dist;

			if (!ply->inuse || !ply->client)
				continue;

			VectorSubtract (ply->s.origin, door->pos_closed, diff);
			dist = (float)VectorLength (diff);
			if (dist < 140.0f)
			{
				player_near = true;
				break;
			}
		}

		if (player_near)
		{
			if (door->state == DOOR_CLOSED || door->state == DOOR_CLOSING)
			{
				door->state = DOOR_OPENING;
				if (door->sound_move)
					G_Sound (ent, CHAN_VOICE, door->sound_move, 1.0f, ATTN_NORM);
			}
		}

		if (door->state == DOOR_OPENING)
		{
			vec3_t delta;
			float dist;
			float step = door->speed * frametime;

			VectorSubtract (door->pos_open, ent->s.origin, delta);
			dist = (float)VectorNormalize (delta);

			if (dist <= step || dist < 0.5f)
			{
				VectorCopy (door->pos_open, ent->s.origin);
				door->state = DOOR_OPEN;
				door->wait_timer = door->wait;
				if (door->sound_stop)
					G_Sound (ent, CHAN_VOICE, door->sound_stop, 1.0f, ATTN_NORM);
			}
			else
			{
				VectorMA (ent->s.origin, step, delta, ent->s.origin);
			}
			gi.linkentity (ent);
		}
		else if (door->state == DOOR_OPEN)
		{
			if (door->wait_timer > 0)
			{
				door->wait_timer -= frametime;
			}
			else if (!player_near)
			{
				door->state = DOOR_CLOSING;
				if (door->sound_move)
					G_Sound (ent, CHAN_VOICE, door->sound_move, 1.0f, ATTN_NORM);
			}
		}
		else if (door->state == DOOR_CLOSING)
		{
			vec3_t delta;
			float dist;
			float step = door->speed * frametime;

			VectorSubtract (door->pos_closed, ent->s.origin, delta);
			dist = (float)VectorNormalize (delta);

			if (dist <= step || dist < 0.5f)
			{
				VectorCopy (door->pos_closed, ent->s.origin);
				door->state = DOOR_CLOSED;
				if (door->sound_stop)
					G_Sound (ent, CHAN_VOICE, door->sound_stop, 1.0f, ATTN_NORM);
			}
			else
			{
				VectorMA (ent->s.origin, step, delta, ent->s.origin);
			}
			gi.linkentity (ent);
		}
	}
}
