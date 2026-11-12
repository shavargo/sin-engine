/*
 * Clean-room GPL implementation of SiN Game Module (API 14)
 * Client lifecycle, movement, weapons, and commands.
 */
#include "g_local.h"

static edict_t *pm_passent;

static trace_t PM_trace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	return gi.trace (start, mins, maxs, end, pm_passent, MASK_PLAYERSOLID);
}

static void SwitchWeapon (edict_t *ent, gclient_local_t *cl, int weapon_idx)
{
	cl->current_weapon = weapon_idx;
	cl->weapon_fire_wait = 0;

	if (!cl->gun_ent)
		return;

	switch (weapon_idx)
	{
	case 0: /* Fists */
		cl->gun_ent->s.modelindex = game.model_view_punch;
		cl->gun_ent->s.anim = 0;
		cl->gun_ent->s.frame = 0;
		ent->client->ps.stats[STAT_SELECTED_ICON] = 0;
		ent->client->ps.stats[STAT_CURRENT_WEAPON] = 0;
		ent->client->ps.stats[STAT_AMMO] = 0;
		ent->client->ps.stats[STAT_CLIPAMMO] = 0;
		break;

	case 1: /* Magnum */
		cl->gun_ent->s.modelindex = game.model_view_magnum;
		cl->gun_ent->s.anim = 0;
		cl->gun_ent->s.frame = 0;
		ent->client->ps.stats[STAT_SELECTED_ICON] = gi.imageindex ("w_magnum");
		ent->client->ps.stats[STAT_CURRENT_WEAPON] = 1;
		ent->client->ps.stats[STAT_AMMO] = 36;
		ent->client->ps.stats[STAT_CLIPAMMO] = 6;
		break;

	case 2: /* Shotgun */
		cl->gun_ent->s.modelindex = game.model_view_shotgun;
		cl->gun_ent->s.anim = 0;
		cl->gun_ent->s.frame = 0;
		ent->client->ps.stats[STAT_SELECTED_ICON] = gi.imageindex ("w_shotgun");
		ent->client->ps.stats[STAT_CURRENT_WEAPON] = 2;
		ent->client->ps.stats[STAT_AMMO] = 24;
		ent->client->ps.stats[STAT_CLIPAMMO] = 8;
		break;

	case 3: /* Assault Rifle */
		cl->gun_ent->s.modelindex = game.model_view_asrifle;
		cl->gun_ent->s.anim = 0;
		cl->gun_ent->s.frame = 0;
		ent->client->ps.stats[STAT_SELECTED_ICON] = gi.imageindex ("w_asrifle");
		ent->client->ps.stats[STAT_CURRENT_WEAPON] = 3;
		ent->client->ps.stats[STAT_AMMO] = 150;
		ent->client->ps.stats[STAT_CLIPAMMO] = 30;
		break;
	}

	gi.linkentity (cl->gun_ent);
}

static void FireWeapon (edict_t *ent, gclient_local_t *cl)
{
	vec3_t forward, right, up, start, end;
	trace_t tr;

	AngleVectors (ent->client->ps.viewangles, forward, right, up);
	VectorCopy (ent->s.origin, start);
	start[2] += (cl->viewheight > 0.0f) ? cl->viewheight : 66.0f;

	switch (cl->current_weapon)
	{
	case 0: /* Fists */
		cl->weapon_fire_wait = 4;
		if (cl->gun_ent)
		{
			cl->gun_ent->s.anim = 4; /* punch1 */
			cl->gun_ent->s.frame = 0;
		}
		if (game.sound_punch_swing)
			G_Sound (ent, CHAN_WEAPON, game.sound_punch_swing, 1.0f, ATTN_NORM);

		VectorMA (start, 64, forward, end);
		tr = gi.trace (start, NULL, NULL, end, ent, MASK_SHOT);
		break;

	case 1: /* Magnum */
		cl->weapon_fire_wait = 5;
		if (cl->gun_ent)
		{
			cl->gun_ent->s.anim = 1; /* fire */
			cl->gun_ent->s.frame = 0;
		}
		if (game.sound_magnum_fire)
			G_Sound (ent, CHAN_WEAPON, game.sound_magnum_fire, 1.0f, ATTN_NORM);

		VectorMA (start, 8192, forward, end);
		tr = gi.trace (start, NULL, NULL, end, ent, MASK_SHOT);
		if (tr.fraction < 1.0f)
		{
			gi.WriteByte (svc_temp_entity);
			gi.WriteByte (TE_GUNSHOT);
			gi.WritePosition (tr.endpos);
			gi.WriteDir (tr.plane.normal);
			gi.multicast (tr.endpos, MULTICAST_PVS);
		}
		if (ent->client->ps.stats[STAT_CLIPAMMO] > 0)
			ent->client->ps.stats[STAT_CLIPAMMO]--;
		if (ent->client->ps.stats[STAT_AMMO] > 0)
			ent->client->ps.stats[STAT_AMMO]--;
		break;

	case 2: /* Shotgun */
		cl->weapon_fire_wait = 8;
		if (cl->gun_ent)
		{
			cl->gun_ent->s.anim = 1; /* fire */
			cl->gun_ent->s.frame = 0;
		}
		if (game.sound_shotgun_fire)
			G_Sound (ent, CHAN_WEAPON, game.sound_shotgun_fire, 1.0f, ATTN_NORM);

		/* Multi-pellet spread */
		for (int p = 0; p < 8; p++)
		{
			float r1 = ((float)(rand() % 100) - 50.0f) * 6.0f;
			float r2 = ((float)(rand() % 100) - 50.0f) * 6.0f;
			vec3_t pellet_end;
			VectorMA (start, 8192, forward, pellet_end);
			VectorMA (pellet_end, r1, right, pellet_end);
			VectorMA (pellet_end, r2, up, pellet_end);

			tr = gi.trace (start, NULL, NULL, pellet_end, ent, MASK_SHOT);
			if (tr.fraction < 1.0f)
			{
				gi.WriteByte (svc_temp_entity);
				gi.WriteByte (TE_GUNSHOT);
				gi.WritePosition (tr.endpos);
				gi.WriteDir (tr.plane.normal);
				gi.multicast (tr.endpos, MULTICAST_PVS);
			}
		}

		if (ent->client->ps.stats[STAT_CLIPAMMO] > 0)
			ent->client->ps.stats[STAT_CLIPAMMO]--;
		if (ent->client->ps.stats[STAT_AMMO] > 0)
			ent->client->ps.stats[STAT_AMMO]--;
		break;

	case 3: /* Assault Rifle */
		cl->weapon_fire_wait = 2;
		if (cl->gun_ent)
		{
			cl->gun_ent->s.anim = 1; /* fire */
			cl->gun_ent->s.frame = 0;
		}
		if (game.sound_asrifle_fire)
			G_Sound (ent, CHAN_WEAPON, game.sound_asrifle_fire, 1.0f, ATTN_NORM);

		VectorMA (start, 8192, forward, end);
		tr = gi.trace (start, NULL, NULL, end, ent, MASK_SHOT);
		if (tr.fraction < 1.0f)
		{
			gi.WriteByte (svc_temp_entity);
			gi.WriteByte (TE_GUNSHOT);
			gi.WritePosition (tr.endpos);
			gi.WriteDir (tr.plane.normal);
			gi.multicast (tr.endpos, MULTICAST_PVS);
		}
		if (ent->client->ps.stats[STAT_CLIPAMMO] > 0)
			ent->client->ps.stats[STAT_CLIPAMMO]--;
		if (ent->client->ps.stats[STAT_AMMO] > 0)
			ent->client->ps.stats[STAT_AMMO]--;
		break;
	}
}

qboolean ClientConnect (edict_t *ent, const char *userinfo)
{
	(void)userinfo;
	ent->client = &game.clients[ent - game.edicts - 1];
	memset (ent->client, 0, sizeof(*ent->client));
	return true;
}

void ClientUserinfoChanged (edict_t *ent, const char *userinfo)
{
	(void)ent;
	(void)userinfo;
}

void ClientBegin (edict_t *ent, qboolean loadgame)
{
	vec3_t spawn_origin, spawn_angles;
	int client_idx = ent - game.edicts - 1;
	gclient_local_t *cl = &game.client_locals[client_idx];

	(void)loadgame;

	ent->inuse = true;
	ent->solid = SOLID_BBOX;
	ent->clipmask = MASK_PLAYERSOLID;

	/* SiN player bounding box: feet at origin */
	VectorSet (ent->mins, -16, -16, 0);
	VectorSet (ent->maxs, 16, 16, 72);

	SelectSpawnPoint (spawn_origin, spawn_angles);

	/* Lift spawn origin slightly above floor as in SiN */
	spawn_origin[2] += 17.0f;

	VectorCopy (spawn_origin, ent->s.origin);
	VectorCopy (spawn_angles, ent->s.angles);
	VectorCopy (spawn_angles, ent->client->ps.viewangles);

	/* Pmove state initialization: origin and velocity scaled by 8 */
	memset (&ent->client->ps.pmove, 0, sizeof(ent->client->ps.pmove));
	ent->client->ps.pmove.pm_type = PM_NORMAL;
	ent->client->ps.pmove.gravity = 800;
	ent->client->ps.pmove.origin[0] = (short)(spawn_origin[0] * 8.0f);
	ent->client->ps.pmove.origin[1] = (short)(spawn_origin[1] * 8.0f);
	ent->client->ps.pmove.origin[2] = (short)(spawn_origin[2] * 8.0f);

	cl->viewheight = 66.0f;
	ent->client->ps.viewoffset[2] = 66.0f;

	/* Status bar values */
	ent->client->ps.stats[STAT_HEALTH] = 100;
	ent->client->ps.stats[STAT_ARMOR] = 50;
	ent->client->ps.stats[STAT_WEAPONLIST] = 0x0f;

	/* Setup view weapon entity parented to player */
	if (!cl->gun_ent)
	{
		cl->gun_ent = G_AllocEdict ();
	}
	if (cl->gun_ent)
	{
		cl->gun_ent->s.number = (int)(cl->gun_ent - game.edicts);
		cl->gun_ent->s.parent = ent->s.number;
		cl->gun_ent->inuse = true;
		cl->gun_ent->solid = SOLID_NOT;
		VectorCopy (ent->s.origin, cl->gun_ent->s.origin);
		VectorCopy (ent->s.angles, cl->gun_ent->s.angles);
	}

	SwitchWeapon (ent, cl, 1); /* Start with Magnum */

	gi.linkentity (ent);
	gi.bprintf (PRINT_HIGH, "Player connected to server\n");
}

void ClientDisconnect (edict_t *ent)
{
	int client_idx;
	gclient_local_t *cl;

	if (!ent)
		return;

	client_idx = ent - game.edicts - 1;
	cl = &game.client_locals[client_idx];

	if (cl->gun_ent)
	{
		G_FreeEdict (cl->gun_ent);
		cl->gun_ent = NULL;
	}

	gi.unlinkentity (ent);
	ent->inuse = false;
}

void ClientCommand (edict_t *ent)
{
	const char *cmd = gi.argv (0);
	int client_idx;
	gclient_local_t *cl;

	if (!cmd || !ent->client)
		return;

	client_idx = ent - game.edicts - 1;
	cl = &game.client_locals[client_idx];

	if (!strcmp (cmd, "say"))
	{
		gi.bprintf (PRINT_CHAT, "Player: %s\n", gi.args ());
		return;
	}

	if (!strcmp (cmd, "use"))
	{
		const char *weap = gi.argv (1);
		if (!weap)
			return;

		if (!strcasecmp (weap, "Fists") || !strcasecmp (weap, "punch"))
		{
			SwitchWeapon (ent, cl, 0);
		}
		else if (!strcasecmp (weap, "Magnum"))
		{
			SwitchWeapon (ent, cl, 1);
		}
		else if (!strcasecmp (weap, "Shotgun"))
		{
			SwitchWeapon (ent, cl, 2);
		}
		else if (!strcasecmp (weap, "Machinegun") || !strcasecmp (weap, "Assault Rifle") || !strcasecmp (weap, "asrifle"))
		{
			SwitchWeapon (ent, cl, 3);
		}
		return;
	}

	if (!strcmp (cmd, "weapnext") || !strcmp (cmd, "weaponuse"))
	{
		int next = (cl->current_weapon + 1) % 4;
		SwitchWeapon (ent, cl, next);
		return;
	}

	if (!strcmp (cmd, "weapprev"))
	{
		int prev = (cl->current_weapon + 3) % 4;
		SwitchWeapon (ent, cl, prev);
		return;
	}

	if (!strcmp (cmd, "spawn"))
	{
		ClientBegin (ent, false);
		return;
	}
}

void ClientThink (edict_t *ent, usercmd_t *cmd)
{
	pmove_t pm;
	int client_idx;
	gclient_local_t *cl;

	if (!ent->inuse || !ent->client)
		return;

	client_idx = ent - game.edicts - 1;
	cl = &game.client_locals[client_idx];

	memset (&pm, 0, sizeof(pm));
	pm.s = ent->client->ps.pmove;
	pm.cmd = *cmd;

	pm_passent = ent;
	pm.trace = PM_trace;
	pm.pointcontents = gi.pointcontents;

	gi.Pmove (&pm);

	ent->client->ps.pmove = pm.s;

	/* Scale fixed-point 12.3 origin and velocity back to float world units */
	ent->s.origin[0] = pm.s.origin[0] * 0.125f;
	ent->s.origin[1] = pm.s.origin[1] * 0.125f;
	ent->s.origin[2] = pm.s.origin[2] * 0.125f;

	cl->velocity[0] = pm.s.velocity[0] * 0.125f;
	cl->velocity[1] = pm.s.velocity[1] * 0.125f;
	cl->velocity[2] = pm.s.velocity[2] * 0.125f;

	cl->viewheight = (float)pm.viewheight;
	ent->client->ps.viewoffset[2] = (float)pm.viewheight;

	VectorCopy (pm.viewangles, ent->s.angles);
	VectorCopy (pm.viewangles, ent->client->ps.viewangles);

	gi.linkentity (ent);

	/* Synchronize viewgun entity position and orientation */
	if (cl->gun_ent && cl->gun_ent->inuse)
	{
		VectorCopy (ent->s.origin, cl->gun_ent->s.origin);
		VectorCopy (ent->s.angles, cl->gun_ent->s.angles);
		gi.linkentity (cl->gun_ent);
	}

	/* Decrement attack cooldown */
	if (cl->weapon_fire_wait > 0)
	{
		cl->weapon_fire_wait--;
	}
	else if (cl->gun_ent && cl->gun_ent->inuse)
	{
		/* Return to ready / idle frame when fire animation finished */
		if (cl->gun_ent->s.anim != 0 && cl->gun_ent->s.anim != 2)
		{
			cl->gun_ent->s.anim = 0;
			cl->gun_ent->s.frame = 0;
		}
	}

	/* Handle firing */
	if ((cmd->buttons & BUTTON_ATTACK) && cl->weapon_fire_wait <= 0)
	{
		FireWeapon (ent, cl);
	}
}
