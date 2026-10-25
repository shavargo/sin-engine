/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

/*
 * cl_modelcmds.c -- client side SiN model (.def) frame commands.
 *
 * Model definition files contain "client <anim> <frame> <command> ..."
 * lines in their !main section.  They drive everything that is purely
 * visual / audible on the client: muzzle flashes, firing sounds, tracers,
 * bullet impacts, rocket exhaust trails, dynamic lights, ...
 *
 * This is an original implementation written from the .def file contents
 * (the grammar is visible in the shipped game data) and the GPL game code.
 *
 * Supported commands:
 *   randomsound / playsound / localsound <alias|file> [vol chan attn ...]
 *   dlight  r rr g rg b rb radius rradius life decay
 *   attach  <bone> <model> [time t] [scale s] [origin f r u] [alphafade]
 *   spawn / bonespawn [<bone>] <model> [time] [scale] [randscale a b]
 *           [vel x y z] [randvel x y z] [absrandvel x y z] [randorigin r]
 *           [randomspawn p] [alphafade] [scaleanim] [animate] [animateonce]
 *   spawnparticle / bonespawnparticle [<bone>] ... count n velocity v
 *   shootbullets [<bone>] n spreadx spready ... range
 */

#include "client.h"

#ifdef SIN_MP

#define Q_strncpyz(d, s, n)	{ strncpy ((d), (s), (n) - 1); (d)[(n) - 1] = 0; }
#define MC_MAX_FX		192
#define MC_RAND()		((rand () & 0x7fff) / (float)0x7fff)
#define MC_CRAND()		(2.0f * (MC_RAND () - 0.5f))

/* ------------------------------------------------------------------ */
/* per-entity animation tracking                                       */
/* ------------------------------------------------------------------ */

typedef struct
{
	int		serverframe;	/* last server frame we processed */
	int		lastseen;		/* last server frame the entity existed */
	int		modelindex;
	int		anim;
	int		frame;
	int		next_loop;		/* cl.time for 1-frame looping anims */
} mc_state_t;

static mc_state_t	mc_state[MAX_EDICTS];

/* ------------------------------------------------------------------ */
/* view gun (positioned by CL_AddViewWeapon each frame)                */
/* ------------------------------------------------------------------ */

static entity_t	mc_gun;
static int		mc_gun_modelindex;
static int		mc_gun_framecount = -1;
static int		mc_view_modelindex;		/* model index of local view weapon */

/* ------------------------------------------------------------------ */
/* transient effects                                                   */
/* ------------------------------------------------------------------ */

typedef struct
{
	qboolean	active;
	qboolean	beam;
	qboolean	follow_gun;		/* positioned relative to the view gun */
	char		bone[32];
	vec3_t		offset;			/* forward, right, up from bone */
	vec3_t		origin;			/* world position (not follow_gun) */
	vec3_t		vel;
	vec3_t		end;			/* beams */
	struct model_s	*model;
	int			start;			/* cl.time */
	int			life;			/* ms */
	float		scale0, scale1;
	int			nframes;
	int			frame;
	qboolean	alphafade, scaleanim, animate, animateonce;
} mc_fx_t;

static mc_fx_t	mc_fx[MC_MAX_FX];

typedef struct
{
	char	path[MAX_QPATH];
	int		nframes;
} mc_sprite_t;

static mc_sprite_t	mc_sprites[32];
static int			mc_num_sprites;

/*
 * Number of frames in a .spr (header: ident, version, numframes).
 */
static int MC_SpriteFrames (const char *path)
{
	int		i, len, n = 1;
	byte	*buf;

	for (i = 0; i < mc_num_sprites; i++)
		if (!Q_stricmp (mc_sprites[i].path, path))
			return mc_sprites[i].nframes;

	len = FS_LoadFile ((char *)path, (void **)&buf);
	if (len >= 12 && buf)
		n = LittleLong (((int *)buf)[2]);
	if (buf)
		FS_FreeFile (buf);
	if (n < 1 || n > 64)
		n = 1;
	if (mc_num_sprites < (int)(sizeof (mc_sprites) / sizeof (mc_sprites[0])))
	{
		Q_strncpyz (mc_sprites[mc_num_sprites].path, path, sizeof (mc_sprites[0].path))
		mc_sprites[mc_num_sprites].nframes = n;
		mc_num_sprites++;
	}
	return n;
}

void CL_MC_Clear (void)
{
	memset (mc_state, 0, sizeof (mc_state));
	memset (mc_fx, 0, sizeof (mc_fx));
	mc_gun_framecount = -1;
	mc_view_modelindex = 0;
}

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */

static mc_fx_t *MC_AllocFx (void)
{
	int		i, oldest = 0;

	for (i = 0; i < MC_MAX_FX; i++)
	{
		if (!mc_fx[i].active)
			return &mc_fx[i];
		if (mc_fx[i].start < mc_fx[oldest].start)
			oldest = i;
	}
	return &mc_fx[oldest];
}

/* context for the command currently being executed */
typedef struct
{
	int			modelindex;
	int			entnum;		/* entity used for sound */
	qboolean	viewgun;
	vec3_t		origin;		/* world origin of the model */
	vec3_t		angles;
	int			anim, frame;
} mc_ctx_t;

/*
 * World position of a named bone, plus the model axes.
 * Model space is x = forward, y = -right, z = up (same as MD2 models).
 */
static qboolean MC_BonePos (const mc_ctx_t *c, const char *bone, vec3_t out, vec3_t f, vec3_t r, vec3_t u)
{
	int		group, tri;
	vec3_t	orient, pos, trans[3];
	const float	*org = c->viewgun ? mc_gun.origin : c->origin;
	const float	*ang = c->viewgun ? mc_gun.angles : c->angles;

	AngleVectors ((float *)ang, f, r, u);
	VectorCopy (org, out);

	if (!bone || !bone[0])
		return true;
	if (!SINMDL_GetBoneInfo (c->modelindex, bone, &group, &tri, orient))
		return false;
	if (!SINMDL_GetBoneTransform (c->modelindex, group, tri, orient, c->anim, c->frame, 1.0f, trans, pos))
		return false;

	VectorMA (out, pos[0], f, out);
	VectorMA (out, -pos[1], r, out);
	VectorMA (out, pos[2], u, out);
	return true;
}

static float MC_Float (const sinmdl_singlecmd_t *cmd, int i)
{
	return (i < cmd->num_args) ? (float)atof (cmd->args[i]) : 0.0f;
}

static qboolean MC_IsNumber (const char *s)
{
	if (!s || !s[0])
		return false;
	if (*s == '-' || *s == '+' || *s == '.')
		s++;
	return (*s >= '0' && *s <= '9') || (*s == '.');
}

/* ------------------------------------------------------------------ */
/* commands                                                            */
/* ------------------------------------------------------------------ */

static void MC_CmdSound (const mc_ctx_t *c, const sinmdl_singlecmd_t *cmd)
{
	const char	*which = cmd->args[0];
	const char	*file = NULL;
	float		vol = 1.0f;
	int			chan = CHAN_WEAPON;
	float		attn = ATTN_NORM;
	struct sfx_s	*sfx;

	if (cmd->num_args < 2)
		return;
	if (!Q_stricmp (which, "localsound") && c->entnum != cl.playernum + 1)
		return;

	if (!Q_stricmp (which, "playsound"))
		file = cmd->args[1];
	else
		file = SINMDL_Alias_FindRandom (c->modelindex, cmd->args[1]);
	if (!file || !file[0])
		return;

	if (cmd->num_args > 2)
		vol = MC_Float (cmd, 2);
	if (cmd->num_args > 3)
		chan = atoi (cmd->args[3]);
	if (cmd->num_args > 4)
		attn = (float)atoi (cmd->args[4]);
	if (vol <= 0.0f)
		vol = 1.0f;

	sfx = S_RegisterSound ((char *)file);
	if (!sfx)
		return;
	S_StartSound (NULL, c->entnum, chan, sfx, vol, attn, 0);
}

static void MC_CmdDlight (const mc_ctx_t *c, const sinmdl_singlecmd_t *cmd)
{
	cdlight_t	*dl;
	vec3_t		f, r, u, org;
	float		life;

	if (cmd->num_args < 11)
		return;

	MC_BonePos (c, "barrel", org, f, r, u);
	if (c->viewgun)
	{
		/* light in front of the muzzle, in view space */
		VectorMA (cl.refdef.vieworg, 24, cl.v_forward, org);
	}

	dl = CL_AllocDlight (0);
	VectorCopy (org, dl->origin);
	dl->color[0] = MC_Float (cmd, 1) + MC_Float (cmd, 2) * MC_RAND ();
	dl->color[1] = MC_Float (cmd, 3) + MC_Float (cmd, 4) * MC_RAND ();
	dl->color[2] = MC_Float (cmd, 5) + MC_Float (cmd, 6) * MC_RAND ();
	dl->radius = MC_Float (cmd, 7) + MC_Float (cmd, 8) * MC_RAND ();
	life = MC_Float (cmd, 9);
	if (life <= 0.0f)
		life = 0.1f;
	dl->die = cl.time + life * 1000.0f;
	dl->decay = dl->radius / life;
	dl->minlight = 0;
}

/*
 * attach <bone> <model> ...   and   spawn / bonespawn
 */
static void MC_CmdSprite (const mc_ctx_t *c, const sinmdl_singlecmd_t *cmd, qboolean attach)
{
	const char	*cname = cmd->args[0];
	qboolean	bonecmd = attach || !Q_stricmp (cname, "bonespawn");
	const char	*bone = NULL;
	const char	*model;
	int			i = 1;
	mc_fx_t		*fx;
	vec3_t		f, r, u, org, vel, randvel, absrandvel, offset;
	float		life = 0.1f, scale = 1.0f, rs0 = 0, rs1 = 0, randorigin = 0, spawnp = 1.0f;
	qboolean	alphafade = false, scaleanim = false, animate = false, once = false;
	int			frame = 0;

	if (bonecmd)
	{
		if (cmd->num_args < 3)
			return;
		bone = cmd->args[i++];
	}
	if (i >= cmd->num_args)
		return;
	model = cmd->args[i++];

	VectorClear (vel);
	VectorClear (randvel);
	VectorClear (absrandvel);
	VectorClear (offset);

	while (i < cmd->num_args)
	{
		const char	*k = cmd->args[i++];

		if (!Q_stricmp (k, "time"))
			life = MC_Float (cmd, i++);
		else if (!Q_stricmp (k, "scale"))
			scale = MC_Float (cmd, i++);
		else if (!Q_stricmp (k, "randscale"))
		{
			rs0 = MC_Float (cmd, i);
			rs1 = MC_Float (cmd, i + 1);
			i += 2;
		}
		else if (!Q_stricmp (k, "origin"))
		{
			offset[0] = MC_Float (cmd, i);
			offset[1] = MC_Float (cmd, i + 1);
			offset[2] = MC_Float (cmd, i + 2);
			i += 3;
		}
		else if (!Q_stricmp (k, "vel"))
		{
			vel[0] = MC_Float (cmd, i);
			vel[1] = MC_Float (cmd, i + 1);
			vel[2] = MC_Float (cmd, i + 2);
			i += 3;
		}
		else if (!Q_stricmp (k, "randvel"))
		{
			randvel[0] = MC_Float (cmd, i);
			randvel[1] = MC_Float (cmd, i + 1);
			randvel[2] = MC_Float (cmd, i + 2);
			i += 3;
		}
		else if (!Q_stricmp (k, "absrandvel"))
		{
			absrandvel[0] = MC_Float (cmd, i);
			absrandvel[1] = MC_Float (cmd, i + 1);
			absrandvel[2] = MC_Float (cmd, i + 2);
			i += 3;
		}
		else if (!Q_stricmp (k, "angles"))
			i += 3;
		else if (!Q_stricmp (k, "randorigin"))
			randorigin = MC_Float (cmd, i++);
		else if (!Q_stricmp (k, "randomspawn"))
			spawnp = MC_Float (cmd, i++);
		else if (!Q_stricmp (k, "frame"))
			frame = atoi (cmd->args[i++]);
		else if (!Q_stricmp (k, "alphafade"))
			alphafade = true;
		else if (!Q_stricmp (k, "scaleanim"))
			scaleanim = true;
		else if (!Q_stricmp (k, "animate"))
			animate = true;
		else if (!Q_stricmp (k, "animateonce"))
			once = true;
		/* detail, physics, touchdie, lerp, randomroll, ...: not needed */
	}

	/* only billboard sprites are supported as effect models */
	if (strlen (model) < 5 || Q_stricmp (model + strlen (model) - 4, ".spr"))
		return;
	if (spawnp < 1.0f && MC_RAND () > spawnp)
		return;

	fx = MC_AllocFx ();
	memset (fx, 0, sizeof (*fx));
	fx->model = re.RegisterModel ((char *)model);
	if (!fx->model)
		return;

	fx->nframes = MC_SpriteFrames (model);
	fx->start = cl.time;
	fx->life = (int)(life * 1000.0f);
	if (fx->life < 50)
		fx->life = 50;
	fx->scale0 = scale;
	if (rs1 > 0)
		fx->scale0 = rs0 + (rs1 - rs0) * MC_RAND ();
	fx->scale1 = fx->scale0;
	fx->frame = frame;
	fx->alphafade = alphafade;
	fx->scaleanim = scaleanim;
	fx->animate = animate || once;
	fx->animateonce = once;
	fx->active = true;

	if (attach && c->viewgun)
	{
		/* stays glued to the view gun barrel */
		fx->follow_gun = true;
		Q_strncpyz (fx->bone, bone, sizeof (fx->bone))
		VectorCopy (offset, fx->offset);
		return;
	}

	if (!MC_BonePos (c, bonecmd ? bone : NULL, org, f, r, u))
	{
		VectorCopy (c->origin, org);
		AngleVectors (c->angles, f, r, u);
	}
	VectorMA (org, offset[0], f, org);
	VectorMA (org, -offset[1], r, org);
	VectorMA (org, offset[2], u, org);
	for (i = 0; i < 3; i++)
		org[i] += randorigin * MC_CRAND ();
	VectorCopy (org, fx->origin);

	VectorClear (fx->vel);
	VectorMA (fx->vel, vel[0] + randvel[0] * MC_CRAND (), f, fx->vel);
	VectorMA (fx->vel, -(vel[1] + randvel[1] * MC_CRAND ()), r, fx->vel);
	VectorMA (fx->vel, vel[2] + randvel[2] * MC_CRAND (), u, fx->vel);
	fx->vel[0] += absrandvel[0] * MC_CRAND ();
	fx->vel[1] += absrandvel[1] * MC_CRAND ();
	fx->vel[2] += absrandvel[2] * MC_CRAND ();
}

static void MC_CmdParticles (const mc_ctx_t *c, const sinmdl_singlecmd_t *cmd)
{
	int			i, count = 10;
	float		vel = 50;
	vec3_t		org, f, r, u, dir;
	const char	*bone = NULL;

	if (!Q_stricmp (cmd->args[0], "bonespawnparticle") && cmd->num_args > 1)
		bone = cmd->args[1];
	for (i = 1; i < cmd->num_args - 1; i++)
	{
		if (!Q_stricmp (cmd->args[i], "count"))
			count = atoi (cmd->args[i + 1]);
		else if (!Q_stricmp (cmd->args[i], "velocity"))
			vel = (float)atof (cmd->args[i + 1]);
	}
	if (!MC_BonePos (c, bone, org, f, r, u))
		VectorCopy (c->origin, org);
	VectorCopy (u, dir);
	if (count > 40)
		count = 40;
	CL_ParticleEffect (org, dir, 0xe0, count);
	(void)vel;
}

static void MC_CmdShootBullets (const mc_ctx_t *c, const sinmdl_singlecmd_t *cmd)
{
	int			i = 1, n, b;
	const char	*bone = NULL;
	float		spreadx, spready, range;
	vec3_t		src, f, r, u, start, end, dir;
	trace_t		tr;

	/* only the local player's own shots are traced on the client */
	if (!c->viewgun)
		return;

	if (i < cmd->num_args && !MC_IsNumber (cmd->args[i]))
		bone = cmd->args[i++];
	n = (i < cmd->num_args) ? atoi (cmd->args[i]) : 1;
	spreadx = MC_Float (cmd, i + 1);
	spready = MC_Float (cmd, i + 2);
	range = MC_Float (cmd, i + 4);
	if (range < 64)
		range = 8192;
	if (n < 1)
		n = 1;
	if (n > 16)
		n = 16;

	/* the tracer starts at the muzzle, the bullet itself leaves the eye */
	if (!MC_BonePos (c, bone ? bone : "barrel", src, f, r, u))
		VectorMA (cl.refdef.vieworg, 16, cl.v_forward, src);

	VectorCopy (cl.refdef.vieworg, start);
	for (b = 0; b < n; b++)
	{
		vec3_t	hitdir;

		VectorMA (start, range, cl.v_forward, end);
		VectorMA (end, spreadx * MC_CRAND (), cl.v_right, end);
		VectorMA (end, spready * MC_CRAND (), cl.v_up, end);

		tr = CL_GenericTrace (start, vec3_origin, vec3_origin, end, MASK_SHOT, true, cl.playernum + 1);
		if (tr.fraction >= 1.0f)
			continue;

		VectorCopy (tr.plane.normal, hitdir);
		CL_ParticleEffect (tr.endpos, hitdir, 0, 6);
		if (b == 0)
			CL_SmokeAndFlash (tr.endpos);

		/* short lived tracer streak */
		if (!(b & 1))
		{
			mc_fx_t	*fx = MC_AllocFx ();

			memset (fx, 0, sizeof (*fx));
			fx->active = true;
			fx->beam = true;
			fx->start = cl.time;
			fx->life = 60;
			VectorCopy (src, fx->origin);
			VectorCopy (tr.endpos, fx->end);
		}
	}
	(void)dir;
}

static void MC_Exec (const mc_ctx_t *c, const sinmdl_singlecmd_t *cmd)
{
	const char	*name;

	if (cmd->num_args < 1 || !cmd->args[0])
		return;
	name = cmd->args[0];

	if (!Q_stricmp (name, "randomsound") || !Q_stricmp (name, "playsound")
		|| !Q_stricmp (name, "localsound"))
		MC_CmdSound (c, cmd);
	else if (!Q_stricmp (name, "dlight"))
		MC_CmdDlight (c, cmd);
	else if (!Q_stricmp (name, "attach"))
		MC_CmdSprite (c, cmd, true);
	else if (!Q_stricmp (name, "spawn") || !Q_stricmp (name, "bonespawn"))
		MC_CmdSprite (c, cmd, false);
	else if (!Q_stricmp (name, "spawnparticle") || !Q_stricmp (name, "bonespawnparticle"))
		MC_CmdParticles (c, cmd);
	else if (!Q_stricmp (name, "shootbullets"))
		MC_CmdShootBullets (c, cmd);
}

static void MC_RunFrame (const mc_ctx_t *c, int anim, int frame)
{
	sinmdl_cmd_t	*cmds;
	int				i;

	cmds = SINMDL_Client_Frame_Commands (c->modelindex, anim, frame);
	if (!cmds)
		return;
	for (i = 0; i < cmds->num_cmds; i++)
		MC_Exec (c, &cmds->cmds[i]);
}

/* ------------------------------------------------------------------ */
/* public interface                                                    */
/* ------------------------------------------------------------------ */

/*
 * Called once per entity per rendered frame from CL_AddPacketEntities.
 * Detects animation/frame changes (once per server frame) and runs the
 * client commands of every frame that was entered.
 */
void CL_MC_ProcessEntity (entity_state_t *s, entity_t *ent, qboolean viewgun)
{
	mc_state_t	*st;
	mc_ctx_t	c;
	int			mi = s->modelindex;
	int			nframes, from, to, f, anim, frame;
	qboolean	fresh;

	if (mi <= 0 || mi >= MAX_MODELS || s->number <= 0 || s->number >= MAX_EDICTS)
		return;
	if (s->parent && !viewgun)
		return;
	if (viewgun)
		mc_view_modelindex = mi;

	st = &mc_state[s->number];
	if (st->serverframe == cl.frame.serverframe)
		return;

	fresh = (st->lastseen != cl.frame.serverframe - 1 && st->lastseen != cl.frame.serverframe)
		|| st->modelindex != mi;
	st->serverframe = cl.frame.serverframe;
	st->lastseen = cl.frame.serverframe;
	if (fresh)
	{
		st->modelindex = mi;
		st->anim = -1;
		st->frame = 0;
		st->next_loop = 0;
	}

	anim = s->anim;
	frame = s->frame;
	nframes = SINMDL_Anim_NumFrames (mi, anim);
	if (nframes <= 0)
	{
		st->anim = anim;
		st->frame = frame;
		return;
	}
	if (frame < 0)
		frame = 0;
	if (frame >= nframes)
		frame = nframes - 1;

	from = -2;		/* nothing to run */
	if (anim != st->anim)
		from = -1;							/* new animation: run 0..frame */
	else if (frame > st->frame)
		from = st->frame;					/* advanced */
	else if (frame < st->frame)
		from = -1;							/* looped / restarted */
	else if (nframes <= 1 && cl.time >= st->next_loop)
		from = -1;							/* single frame anim: re-trigger on its own clock */

	if (from == -2)
		return;

	if (nframes <= 1)
	{
		float	t = SINMDL_Anim_Time (mi, anim);

		if (t < 0.1f)
			t = 0.1f;
		st->next_loop = cl.time + (int)(t * 1000.0f);
	}
	st->anim = anim;
	st->frame = frame;

	memset (&c, 0, sizeof (c));
	c.modelindex = mi;
	c.entnum = viewgun ? cl.playernum + 1 : s->number;
	c.viewgun = viewgun;
	VectorCopy (ent->origin, c.origin);
	VectorCopy (ent->angles, c.angles);
	c.anim = anim;
	c.frame = frame;

	to = frame;
	if (to - from > 24)
		from = to - 24;
	for (f = from + 1; f <= to; f++)
		MC_RunFrame (&c, anim, f);
}

/*
 * Remember where the view gun was drawn this frame (for muzzle effects).
 */
void CL_MC_SetViewGun (entity_t *gun)
{
	mc_gun = *gun;
	mc_gun_modelindex = mc_view_modelindex;
	mc_gun_framecount = cls.framecount;
}

/*
 * Submit all live effects to the renderer.
 */
void CL_MC_AddEffects (void)
{
	int		i;

	for (i = 0; i < MC_MAX_FX; i++)
	{
		mc_fx_t		*fx = &mc_fx[i];
		entity_t	ent;
		float		frac, age;
		vec3_t		f, r, u;

		if (!fx->active)
			continue;
		age = (float)(cl.time - fx->start);
		if (age < 0 || age > fx->life)
		{
			fx->active = false;
			continue;
		}
		frac = age / (float)fx->life;

		memset (&ent, 0, sizeof (ent));

		if (fx->beam)
		{
			VectorCopy (fx->origin, ent.origin);
			VectorCopy (fx->end, ent.oldorigin);
			ent.flags = RF_BEAM | RF_TRANSLUCENT;
			ent.alpha = 0.35f * (1.0f - frac);
			ent.frame = 1;
			ent.skinnum = 0xe0;
			V_AddEntity (&ent);
			continue;
		}

		if (fx->follow_gun)
		{
			mc_ctx_t	c;

			if (mc_gun_framecount != cls.framecount)
			{
				fx->active = false;
				continue;
			}
			memset (&c, 0, sizeof (c));
			c.modelindex = mc_gun_modelindex;
			c.viewgun = true;
			c.anim = mc_gun.sin_anim;
			c.frame = mc_gun.frame;
			if (!MC_BonePos (&c, fx->bone, ent.origin, f, r, u))
				VectorCopy (mc_gun.origin, ent.origin);
			VectorMA (ent.origin, fx->offset[0], f, ent.origin);
			VectorMA (ent.origin, -fx->offset[1], r, ent.origin);
			VectorMA (ent.origin, fx->offset[2], u, ent.origin);
			ent.flags |= RF_DEPTHHACK;
		}
		else
		{
			VectorMA (fx->origin, age * 0.001f, fx->vel, ent.origin);
		}
		VectorCopy (ent.origin, ent.oldorigin);

		ent.model = fx->model;
		ent.scale = fx->scale0;
		if (fx->scaleanim)
			ent.scale = fx->scale0 * (0.1f + 0.9f * frac);
		if (fx->animate && fx->nframes > 1)
		{
			if (fx->animateonce)
				ent.frame = (int)(frac * fx->nframes);
			else
				ent.frame = (int)(age * 0.01f);
			if (ent.frame >= fx->nframes)
				ent.frame = fx->animateonce ? fx->nframes - 1 : ent.frame % fx->nframes;
		}
		else
			ent.frame = fx->frame;

		if (fx->alphafade)
		{
			ent.flags |= RF_TRANSLUCENT;
			ent.alpha = 1.0f - frac;
		}
		V_AddEntity (&ent);
	}
}

#endif /* SIN_MP */
