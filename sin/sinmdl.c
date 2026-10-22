#include "sinmdl.h"

#ifdef SIN_MP

/*
 * ref_gl links a copy for SINMDL_Init only (draw path TODO).
 * Dedicated / client engine owns the real .def/.sam cache via FS_LoadFile.
 */
#ifdef REF_GL

void SINMDL_Init (void)
{
}

void SINMDL_Shutdown (void)
{
}

#else /* !REF_GL — engine */

#include "../qcommon/qcommon.h"

#ifdef _WIN32
#define sin_strcasecmp	_stricmp
#define sin_strncasecmp	_strnicmp
#else
#define sin_strcasecmp	strcasecmp
#define sin_strncasecmp	strncasecmp
#endif

#define SINMDL_MAX_DEFS		512
#define SINMDL_MAX_ANIMS	192
#define SINMDL_MAX_SKINS	16
#define SINMDL_MAX_BONES	32
#define SINMDL_MAX_FCMDS	512
#define SINMDL_NAME_LEN		64
#define SAM_MAGIC			0x204d4153u	/* 'SAM ' */
#define SBM_MAGIC			0x204d4253u	/* 'SBM ' */
#define SDEF_MAGIC			0x46454453u	/* 'SDEF' */

typedef struct
{
	char	name[SINMDL_NAME_LEN];
	char	file[MAX_QPATH];		/* full path for bone resolve */
	int		num_frames;
	float	time;
	float	weight;
	vec3_t	mins, maxs;
} sinmdl_anim_t;

typedef struct
{
	char	name[SINMDL_NAME_LEN];	/* bone name e.g. barrel */
	char	group_name[SINMDL_NAME_LEN];
	int		def_id;					/* DEF id N */
	int		surf_index;				/* matching SBM surface index */
	int		tri_num;
	vec3_t	orientation;			/* pitch yaw roll from DEF */
	vec3_t	local_pos;				/* model-space attach from SAM f0 */
	vec3_t	tri_v[3];				/* unscaled SAM f0 triangle vertices (plus d->origin) */
	qboolean	have_pos;
	qboolean	have_tri_v;
} sinmdl_bone_t;

#define SINMDL_MAX_GROUPS	32

typedef struct
{
	char	defpath[MAX_QPATH];
	char	asset_path[MAX_QPATH];
	char	sbm_name[SINMDL_NAME_LEN];
	float	scale;
	vec3_t	origin;
	int		num_anims;
	sinmdl_anim_t	anims[SINMDL_MAX_ANIMS];
	int		num_skins;
	char	skins[SINMDL_MAX_SKINS][SINMDL_NAME_LEN];
	int		num_bones;
	sinmdl_bone_t	bones[SINMDL_MAX_BONES];
	int		num_groups;				/* de-duplicated DEF group count */
	char	groups[SINMDL_MAX_GROUPS][SINMDL_NAME_LEN];
	vec3_t	mins, maxs;
	sinmdl_cmd_t	init_cmds;
	/* !main frame commands: server|client ANIM first|last|N CMD ... */
	int		num_fcmds;
	struct
	{
		char	anim[SINMDL_NAME_LEN];
		int		frame;		/* 0=first, -1=last, else exact */
		qboolean	server;
		int		num_args;
		char	*args[SINMDL_CMD_MAX_ARGS];
	}		fcmds[SINMDL_MAX_FCMDS];
	/* Was 16 tokens — weapon defs need far more (classname/etc). Cap truncates → game calls NULL. */
	char	init_arg_store[2048][64];
	int		init_arg_used;
	qboolean	used;
} sinmdl_def_t;

static sinmdl_def_t	sinmdl_defs[SINMDL_MAX_DEFS];
/* modelindex → def slot+1 (0 = none) */
static int			sinmdl_index_map[MAX_MODELS];

static qboolean
name_ends_with (const char *name, const char *ext)
{
	size_t	n, e;

	if (!name || !ext)
		return false;
	n = strlen (name);
	e = strlen (ext);
	if (n < e)
		return false;
	return !sin_strcasecmp (name + n - e, ext);
}

static void
join_asset (char *out, size_t outsz, const char *dir, const char *file)
{
	if (dir && dir[0])
		Com_sprintf (out, (int)outsz, "%s/%s", dir, file);
	else
		Com_sprintf (out, (int)outsz, "%s", file);
}

static qboolean
load_sam_anim (sinmdl_anim_t *anim, const char *path)
{
	byte			*buf;
	int				len;
	const unsigned	*dw;
	const float		*fl;
	const char		*nm;

	len = FS_LoadFile ((char *)path, (void **)&buf);
	if (len < 128 || !buf)
		return false;
	dw = (const unsigned *)buf;
	fl = (const float *)buf;
	if (dw[0] != SAM_MAGIC || dw[1] != 1)
	{
		FS_FreeFile (buf);
		return false;
	}
	nm = (const char *)buf + 8;
	strncpy (anim->name, nm, SINMDL_NAME_LEN - 1);
	anim->name[SINMDL_NAME_LEN - 1] = 0;
	anim->time = fl[0x1b];
	anim->weight = 1.0f;
	anim->num_frames = (int)dw[0x1d];
	anim->mins[0] = fl[0x12];
	anim->mins[1] = fl[0x13];
	anim->mins[2] = fl[0x14];
	anim->maxs[0] = fl[0x15];
	anim->maxs[1] = fl[0x16];
	anim->maxs[2] = fl[0x17];
	FS_FreeFile (buf);
	return anim->num_frames >= 1;
}

static char *
def_store_arg (sinmdl_def_t *d, const char *tok)
{
	int	cap = (int)(sizeof (d->init_arg_store) / sizeof (d->init_arg_store[0]));

	if (d->init_arg_used >= cap)
		return NULL;
	strncpy (d->init_arg_store[d->init_arg_used], tok, 63);
	d->init_arg_store[d->init_arg_used][63] = 0;
	return d->init_arg_store[d->init_arg_used++];
}

/*
 * Quote-aware tokenizer matching sin.exe COM_Parse behavior on a single line:
 * strips surrounding double-quotes and keeps spaces inside quotes as one token.
 */
static int
tokenize_sdef_line (char *line, char **words, int max_words)
{
	char	*p = line;
	int		nw = 0;

	while (*p && nw < max_words)
	{
		while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
			p++;
		if (!*p)
			break;
		if (*p == '"')
		{
			p++;
			words[nw++] = p;
			while (*p && *p != '"')
				p++;
			if (*p == '"')
				*p++ = 0;
		}
		else
		{
			words[nw++] = p;
			while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
				p++;
			if (*p)
				*p++ = 0;
		}
	}
	return nw;
}

static void
def_add_init_cmd_words (sinmdl_def_t *d, char **words, int nw)
{
	sinmdl_singlecmd_t	*cmd;
	char				*stored;
	int					i;

	if (nw < 1 || d->init_cmds.num_cmds >= SINMDL_CMD_MAX_CMDS)
		return;
	cmd = &d->init_cmds.cmds[d->init_cmds.num_cmds];
	cmd->num_args = 0;
	for (i = 0; i < nw && cmd->num_args < SINMDL_CMD_MAX_ARGS; i++)
	{
		stored = def_store_arg (d, words[i]);
		if (!stored)
			break;
		cmd->args[cmd->num_args++] = stored;
	}
	if (cmd->num_args > 0)
		d->init_cmds.num_cmds++;
}

/* !main: [server|client] <anim> first|start|last|end|<n> <cmd> [args...] */
static void
def_add_frame_cmd_words (sinmdl_def_t *d, char **words, int nw, qboolean server)
{
	char	*stored;
	int		i, frame;

	if (d->num_fcmds >= SINMDL_MAX_FCMDS)
		return;
	/* need anim + frame + at least one cmd token */
	if (nw < 3)
		return;
	if (!sin_strcasecmp (words[1], "first") || !sin_strcasecmp (words[1], "start"))
		frame = 0;
	else if (!sin_strcasecmp (words[1], "last") || !sin_strcasecmp (words[1], "end"))
		frame = -1;
	else
		frame = atoi (words[1]);

	memset (&d->fcmds[d->num_fcmds], 0, sizeof (d->fcmds[0]));
	strncpy (d->fcmds[d->num_fcmds].anim, words[0], SINMDL_NAME_LEN - 1);
	d->fcmds[d->num_fcmds].frame = frame;
	d->fcmds[d->num_fcmds].server = server;
	for (i = 2; i < nw && d->fcmds[d->num_fcmds].num_args < SINMDL_CMD_MAX_ARGS; i++)
	{
		stored = def_store_arg (d, words[i]);
		if (!stored)
			break;
		d->fcmds[d->num_fcmds].args[d->fcmds[d->num_fcmds].num_args++] = stored;
	}
	if (d->fcmds[d->num_fcmds].num_args > 0)
		d->num_fcmds++;
}

static qboolean
parse_sdef (sinmdl_def_t *d, char *text, int len)
{
	char	*p = text;
	char	*end = text + len;
	char	line[256];
	char	full[MAX_QPATH];
	int		in_init = 0;
	int		in_main = 0;
	int		i, j, a;

	if (len < 4 || *(unsigned *)text != SDEF_MAGIC)
		return false;
	p += 4;
	d->scale = 1.f;
	VectorClear (d->mins);
	VectorClear (d->maxs);

	/* Blank out multi-line block comments (preserving newlines) */
	{
		char	*c = p;
		while (c + 1 < end)
		{
			if (c[0] == '/' && c[1] == '*')
			{
				c[0] = ' ';
				c[1] = ' ';
				c += 2;
				while (c < end)
				{
					if (c + 1 < end && c[0] == '*' && c[1] == '/')
					{
						c[0] = ' ';
						c[1] = ' ';
						c += 2;
						break;
					}
					if (*c != '\r' && *c != '\n')
						*c = ' ';
					c++;
				}
			}
			else
			{
				c++;
			}
		}
	}

	while (p < end)
	{
		char	*nl;
		char	*words[SINMDL_CMD_MAX_ARGS + 8];
		int		nw;
		size_t	L;

		while (p < end && (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t'))
			p++;
		if (p >= end)
			break;
		nl = p;
		while (nl < end && *nl != '\n' && *nl != '\r')
			nl++;
		L = (size_t)(nl - p);
		if (L >= sizeof (line))
			L = sizeof (line) - 1;
		memcpy (line, p, L);
		line[L] = 0;
		p = nl;
		{
			qboolean	in_q = false;
			char		*c;
			for (c = line; *c; c++)
			{
				if (*c == '"')
					in_q = !in_q;
				else if (!in_q && c[0] == '/' && c[1] == '/')
				{
					*c = 0;
					break;
				}
			}
		}
		L = strlen (line);
		while (L > 0 && (line[L - 1] == ' ' || line[L - 1] == '\t'))
			line[--L] = 0;
		if (!line[0] || line[0] == '*')
			continue;
		if (line[0] == '!')
		{
			in_init = !sin_strncasecmp (line, "!init", 5);
			in_main = !sin_strncasecmp (line, "!main", 5);
			continue;
		}

		nw = tokenize_sdef_line (line, words, (int)(sizeof (words) / sizeof (words[0])));
		if (nw < 1)
			continue;

		if (in_init)
		{
			/*
			 * sin.exe (0x00488AF0): InitCommands returns ONLY server init commands
			 * (plus bare commands in !init: with neither server nor client prefix).
			 */
			if (!sin_strcasecmp (words[0], "client"))
				continue;
			if (!sin_strcasecmp (words[0], "server"))
				def_add_init_cmd_words (d, words + 1, nw - 1);
			else
				def_add_init_cmd_words (d, words, nw);
			continue;
		}
		if (in_main)
		{
			if (!sin_strcasecmp (words[0], "client"))
				def_add_frame_cmd_words (d, words + 1, nw - 1, false);
			else if (!sin_strcasecmp (words[0], "server"))
				def_add_frame_cmd_words (d, words + 1, nw - 1, true);
			else
				def_add_frame_cmd_words (d, words, nw, true);
			continue;
		}

		if (!sin_strcasecmp (words[0], "path") && nw >= 2)
			strncpy (d->asset_path, words[1], sizeof (d->asset_path) - 1);
		else if (!sin_strcasecmp (words[0], "scale") && nw >= 2)
			d->scale = (float)atof (words[1]);
		else if (!sin_strcasecmp (words[0], "origin"))
		{
			d->origin[0] = (nw > 1) ? (float)atof (words[1]) : 0.f;
			d->origin[1] = (nw > 2) ? (float)atof (words[2]) : 0.f;
			d->origin[2] = (nw > 3) ? (float)atof (words[3]) : 0.f;
		}
		else if (!sin_strcasecmp (words[0], "id") && nw >= 4 && d->num_bones < SINMDL_MAX_BONES)
		{
			/* id N group G [bone B [pitch yaw roll] [flags...]] */
			sinmdl_bone_t	*b = &d->bones[d->num_bones];

			if (!sin_strcasecmp (words[2], "group"))
			{
				int	g;

				/* De-duplicate group names in declaration order (sin.exe 0x00487B77) */
				for (g = 0; g < d->num_groups; g++)
				{
					if (!sin_strcasecmp (d->groups[g], words[3]))
						break;
				}
				if (g == d->num_groups && d->num_groups < SINMDL_MAX_GROUPS)
				{
					strncpy (d->groups[d->num_groups], words[3], SINMDL_NAME_LEN - 1);
					d->groups[d->num_groups][SINMDL_NAME_LEN - 1] = 0;
					d->num_groups++;
				}

				memset (b, 0, sizeof (*b));
				b->def_id = atoi (words[1]);
				strncpy (b->group_name, words[3], SINMDL_NAME_LEN - 1);
				if (nw >= 6 && !sin_strcasecmp (words[4], "bone"))
				{
					strncpy (b->name, words[5], SINMDL_NAME_LEN - 1);
					b->orientation[0] = (nw > 6) ? (float)atof (words[6]) : 0.f;
					b->orientation[1] = (nw > 7) ? (float)atof (words[7]) : 0.f;
					b->orientation[2] = (nw > 8) ? (float)atof (words[8]) : 0.f;
				}
				b->surf_index = -1;
				b->tri_num = 0;
				d->num_bones++;
			}
		}
		else
		{
			const char	*sam = NULL;
			const char	*tga = NULL;
			const char	*sbm = NULL;
			const char	*label = NULL;
			char		bare_label[SINMDL_NAME_LEN];
			float		weight = 1.0f;

			/*
			 * sin.exe (0x004880A2):
			 * 3 tokens: <label> <file.sam> <weight>
			 * 2 tokens: <label> <file.sam>
			 * 1 token:  <file.sam> (label = filename minus .sam)
			 */
			if (nw >= 2 && name_ends_with (words[1], "sam"))
			{
				label = words[0];
				sam = words[1];
				if (nw >= 3)
				{
					weight = (float)atof (words[2]);
					if (weight <= 0.f)
						weight = 1.0f;
				}
			}
			else if (name_ends_with (words[0], "sam"))
			{
				size_t	sl = strlen (words[0]);
				sam = words[0];
				if (sl > 4)
				{
					if (sl - 4 >= sizeof (bare_label))
						sl = sizeof (bare_label) + 3;
					memcpy (bare_label, words[0], sl - 4);
					bare_label[sl - 4] = 0;
					label = bare_label;
				}
			}

			if (nw >= 2 && name_ends_with (words[1], "tga"))
				tga = words[1];
			else if (name_ends_with (words[0], "tga"))
				tga = words[0];

			if (nw >= 2 && name_ends_with (words[1], "sbm"))
				sbm = words[1];
			else if (name_ends_with (words[0], "sbm"))
				sbm = words[0];

			if (sbm && !d->sbm_name[0])
				strncpy (d->sbm_name, sbm, SINMDL_NAME_LEN - 1);
			else if (sam)
			{
				join_asset (full, sizeof (full), d->asset_path, sam);
				if (d->num_anims < SINMDL_MAX_ANIMS && load_sam_anim (&d->anims[d->num_anims], full))
				{
					strncpy (d->anims[d->num_anims].file, full, sizeof (d->anims[0].file) - 1);
					d->anims[d->num_anims].weight = weight;
					if (label && label[0])
					{
						strncpy (d->anims[d->num_anims].name, label, SINMDL_NAME_LEN - 1);
						d->anims[d->num_anims].name[SINMDL_NAME_LEN - 1] = 0;
					}
					d->num_anims++;
				}
			}
			else if (tga && d->num_skins < SINMDL_MAX_SKINS)
			{
				join_asset (d->skins[d->num_skins], SINMDL_NAME_LEN, d->asset_path, tga);
				d->num_skins++;
			}
		}
	}

	/*
	 * sin.exe (0x00487010): Sort animations alphabetically by _stricmp so
	 * server animation indices match client animation indices on the wire.
	 */
	for (i = 0; i < d->num_anims - 1; i++)
	{
		for (j = i + 1; j < d->num_anims; j++)
		{
			if (sin_strcasecmp (d->anims[i].name, d->anims[j].name) > 0)
			{
				sinmdl_anim_t	tmp_anim = d->anims[i];
				d->anims[i] = d->anims[j];
				d->anims[j] = tmp_anim;
			}
		}
	}

	for (i = 0; i < d->num_anims; i++)
	{
		for (a = 0; a < 3; a++)
		{
			float	mn = d->anims[i].mins[a];
			float	mx = d->anims[i].maxs[a];
			if (i == 0 || mn < d->mins[a])
				d->mins[a] = mn;
			if (i == 0 || mx > d->maxs[a])
				d->maxs[a] = mx;
		}
	}
	return d->num_anims > 0 || d->num_skins > 0 || d->num_bones > 0;
}

/*
 * Map DEF bone id → SBM surface; cache tri0 centroid from SAM frame 0.
 * Enough for muzzle attach (GetBoneInfo / GetBoneTransform).
 */
static void
resolve_bones (sinmdl_def_t *d)
{
	byte			*sbm = NULL, *sam = NULL;
	int				sbm_len, sam_len, s, b, ai;
	char			path[MAX_QPATH];
	const unsigned	*dw;
	int				nxyz, nsurfs;
	unsigned short	tri_xyz[SINMDL_MAX_BONES][3];
	qboolean		have_tri[SINMDL_MAX_BONES];
	float			sx, sy, sz, tx, ty, tz;
	const byte		*verts;
	int				fofs, rel, nv, nframes;

	if (!d || d->num_bones < 1 || !d->sbm_name[0])
		return;

	memset (have_tri, 0, sizeof (have_tri));
	join_asset (path, sizeof (path), d->asset_path, d->sbm_name);
	sbm_len = FS_LoadFile (path, (void **)&sbm);
	if (sbm_len < 28 || !sbm)
		return;
	dw = (const unsigned *)sbm;
	if (dw[0] != SBM_MAGIC || dw[1] != 1)
	{
		FS_FreeFile (sbm);
		return;
	}
	nxyz = (int)dw[2];
	nsurfs = (int)dw[4];
	if (nxyz < 1 || nsurfs < 1 || nsurfs > 64)
	{
		FS_FreeFile (sbm);
		return;
	}
	if (d->num_groups == 0)
		d->num_groups = nsurfs;

	for (b = 0; b < d->num_bones; b++)
	{
		sinmdl_bone_t	*bone = &d->bones[b];

		bone->surf_index = -1;
		for (s = 0; s < nsurfs; s++)
		{
			int	so = 28 + s * 24;
			int	sid;

			if (so + 24 > sbm_len)
				break;
			/* SBM surf id is DEF id - 1 (REA / pak samples: 200→199, 210→209). */
			sid = LittleLong (*(int *)(sbm + so));
			if (sid != bone->def_id - 1 && sid != bone->def_id)
				continue;
			{
				int			ntris = LittleLong (*(int *)(sbm + so + 4));
				int			tofs = LittleLong (*(int *)(sbm + so + 16));
				const byte	*tr;

				bone->surf_index = s;
				if (ntris < 1 || tofs < 0 || so + tofs + 16 > sbm_len)
					break;
				tr = sbm + so + tofs;
				/* sin.exe (0x0048A104): SBM triangle is 3 consecutive xyz shorts at +0, +2, +4 */
				tri_xyz[b][0] = (unsigned short)LittleShort (*(short *)(tr + 0));
				tri_xyz[b][1] = (unsigned short)LittleShort (*(short *)(tr + 2));
				tri_xyz[b][2] = (unsigned short)LittleShort (*(short *)(tr + 4));
				if (tri_xyz[b][0] < (unsigned)nxyz
					&& tri_xyz[b][1] < (unsigned)nxyz
					&& tri_xyz[b][2] < (unsigned)nxyz)
					have_tri[b] = true;
				bone->tri_num = 0;
				break;
			}
		}
	}
	FS_FreeFile (sbm);

	ai = 0;
	for (b = 0; b < d->num_anims; b++)
	{
		if (strstr (d->anims[b].name, "idle") || strstr (d->anims[b].name, "ready"))
		{
			ai = b;
			break;
		}
	}
	if (d->num_anims >= 1 && d->anims[ai].file[0]
		&& (sam_len = FS_LoadFile (d->anims[ai].file, (void **)&sam)) >= 0x80
		&& sam)
	{
		dw = (const unsigned *)sam;
		if (dw[0] == SAM_MAGIC && dw[1] == 1)
		{
			nv = (int)dw[0x1c];
			nframes = (int)dw[0x1d];
			fofs = (int)dw[0x1e];
			if (nv == nxyz && nframes >= 1 && fofs >= 0 && fofs + 0x2c <= sam_len)
			{
				sx = LittleFloat (*(float *)(sam + fofs + 0x10));
				sy = LittleFloat (*(float *)(sam + fofs + 0x14));
				sz = LittleFloat (*(float *)(sam + fofs + 0x18));
				tx = LittleFloat (*(float *)(sam + fofs + 0x1c)) + d->origin[0];
				ty = LittleFloat (*(float *)(sam + fofs + 0x20)) + d->origin[1];
				tz = LittleFloat (*(float *)(sam + fofs + 0x24)) + d->origin[2];
				rel = LittleLong (*(int *)(sam + fofs + 0x28));
				if (rel >= 0 && fofs + rel + nv * 4 <= sam_len)
				{
					verts = sam + fofs + rel;
					for (b = 0; b < d->num_bones; b++)
					{
						sinmdl_bone_t	*bone = &d->bones[b];
						float			ax = 0.f, ay = 0.f, az = 0.f;
						int				v;

						if (!have_tri[b])
							continue;
						for (v = 0; v < 3; v++)
						{
							const byte	*p = verts + tri_xyz[b][v] * 4;
							float		vx = (float)p[0] * sx + tx;
							float		vy = (float)p[1] * sy + ty;
							float		vz = (float)p[2] * sz + tz;

							bone->tri_v[v][0] = vx;
							bone->tri_v[v][1] = vy;
							bone->tri_v[v][2] = vz;
							ax += vx;
							ay += vy;
							az += vz;
						}
						bone->local_pos[0] = ax * (1.f / 3.f);
						bone->local_pos[1] = ay * (1.f / 3.f);
						bone->local_pos[2] = az * (1.f / 3.f);
						bone->have_pos = true;
						bone->have_tri_v = true;
					}
				}
			}
		}
		FS_FreeFile (sam);
	}

	/* Surf matched — GetBoneInfo OK even if SAM centroid missing. */
	for (b = 0; b < d->num_bones; b++)
	{
		if (d->bones[b].surf_index >= 0 && !d->bones[b].have_pos)
		{
			VectorClear (d->bones[b].local_pos);
			d->bones[b].have_pos = true;
		}
	}
}

static sinmdl_def_t *
def_for_index (int modelindex)
{
	int	h;

	if (modelindex <= 0 || modelindex >= MAX_MODELS)
		return NULL;
	h = sinmdl_index_map[modelindex] - 1;
	if (h < 0 || h >= SINMDL_MAX_DEFS || !sinmdl_defs[h].used)
		return NULL;
	return &sinmdl_defs[h];
}

static int
register_def (const char *defpath)
{
	byte	*buf;
	int		len, i;

	for (i = 0; i < SINMDL_MAX_DEFS; i++)
	{
		if (sinmdl_defs[i].used && !sin_strcasecmp (sinmdl_defs[i].defpath, defpath))
			return i;
	}
	for (i = 0; i < SINMDL_MAX_DEFS; i++)
	{
		if (!sinmdl_defs[i].used)
			break;
	}
	if (i >= SINMDL_MAX_DEFS)
	{
		Com_Printf ("SINMDL_RegisterDef: cache full (%s)\n", defpath);
		return -1;
	}
	len = FS_LoadFile ((char *)defpath, (void **)&buf);
	if (len < 0 || !buf)
	{
		Com_DPrintf ("SINMDL: could not load %s\n", defpath);
		return -1;
	}
	memset (&sinmdl_defs[i], 0, sizeof (sinmdl_defs[i]));
	strncpy (sinmdl_defs[i].defpath, defpath, sizeof (sinmdl_defs[i].defpath) - 1);
	if (!parse_sdef (&sinmdl_defs[i], (char *)buf, len))
	{
		FS_FreeFile (buf);
		return -1;
	}
	FS_FreeFile (buf);
	resolve_bones (&sinmdl_defs[i]);
	sinmdl_defs[i].used = true;
	{
		int	resolved = 0, b;

		for (b = 0; b < sinmdl_defs[i].num_bones; b++)
			if (sinmdl_defs[i].bones[b].surf_index >= 0)
				resolved++;
		Com_DPrintf ("SINMDL: %s anims=%d bones=%d/%d fcmds=%d scale=%.2f\n",
			defpath, sinmdl_defs[i].num_anims, resolved,
			sinmdl_defs[i].num_bones, sinmdl_defs[i].num_fcmds,
			sinmdl_defs[i].scale);
	}
	return i;
}

void SINMDL_Init (void)
{
	memset (sinmdl_defs, 0, sizeof (sinmdl_defs));
	memset (sinmdl_index_map, 0, sizeof (sinmdl_index_map));
	Com_Printf ("SINMDL: def/sam bounds ready\n");
}

void SINMDL_Shutdown (void)
{
	memset (sinmdl_defs, 0, sizeof (sinmdl_defs));
	memset (sinmdl_index_map, 0, sizeof (sinmdl_index_map));
}

void SINMDL_RegisterDefForIndex (int modelindex, const char *defpath)
{
	int	h;

	if (modelindex <= 0 || modelindex >= MAX_MODELS || !defpath)
		return;
	if (!name_ends_with (defpath, "def"))
		return;
	if (sinmdl_index_map[modelindex])
		return;
	h = register_def (defpath);
	if (h >= 0)
		sinmdl_index_map[modelindex] = h + 1;
}

qboolean SINMDL_IsModel (int modelindex)
{
	return def_for_index (modelindex) != NULL;
}

void SINMDL_CalculateBounds (int modelindex, float scale, vec3_t mins, vec3_t maxs)
{
	sinmdl_def_t	*d = def_for_index (modelindex);
	int				a;

	if (!d)
	{
		mins[0] = mins[1] = -16.f;
		mins[2] = 0.f;
		maxs[0] = maxs[1] = 16.f;
		maxs[2] = 32.f;
		return;
	}
	if (scale <= 0.f)
		scale = 1.f;
	scale *= (d->scale != 0.f) ? d->scale : 1.f;
	for (a = 0; a < 3; a++)
	{
		mins[a] = d->mins[a] * scale;
		maxs[a] = d->maxs[a] * scale;
	}
}

int SINMDL_NumAnims (int modelindex)
{
	sinmdl_def_t	*d = def_for_index (modelindex);
	return d ? d->num_anims : 0;
}

int SINMDL_NumSkins (int modelindex)
{
	sinmdl_def_t	*d = def_for_index (modelindex);
	return d ? d->num_skins : 0;
}

int SINMDL_NumGroups (int modelindex)
{
	sinmdl_def_t	*d = def_for_index (modelindex);
	return d ? d->num_groups : 0;
}

sinmdl_cmd_t *SINMDL_InitCommands (int modelindex)
{
	static sinmdl_cmd_t	empty;
	sinmdl_def_t	*d = def_for_index (modelindex);

	return d ? &d->init_cmds : &empty;
}

sinmdl_cmd_t *SINMDL_Frame_Commands (int modelindex, int animnum, int framenum)
{
	static sinmdl_cmd_t	ring[16];
	static int			ri;
	static sinmdl_cmd_t	empty;
	sinmdl_def_t		*d = def_for_index (modelindex);
	sinmdl_cmd_t		*out;
	const char			*aname;
	int					i, nframes, want;

	out = &ring[ri++ & 15];
	memset (out, 0, sizeof (*out));
	if (!d || animnum < 0 || animnum >= d->num_anims)
		return &empty;

	aname = d->anims[animnum].name;
	nframes = d->anims[animnum].num_frames;
	if (nframes < 1)
		nframes = 1;
	want = framenum;
	if (want < 0)
		want = 0;
	if (want >= nframes)
		want = nframes - 1;

	for (i = 0; i < d->num_fcmds; i++)
	{
		const char	*fa = d->fcmds[i].anim;
		int			f = d->fcmds[i].frame;
		int			a;
		qboolean	match = false;

		if (!d->fcmds[i].server)
			continue;
		if (!sin_strcasecmp (fa, aname))
			match = true;
		else if (!sin_strncasecmp (fa, "primary", 7) && !sin_strcasecmp (fa + 7, aname))
			match = true;
		else if (!sin_strncasecmp (fa, "secondary", 9) && !sin_strcasecmp (fa + 9, aname))
			match = true;
		if (!match)
			continue;
		if (f == -1)
			f = nframes - 1;
		if (f != want)
			continue;
		if (out->num_cmds >= SINMDL_CMD_MAX_CMDS)
			break;
		out->cmds[out->num_cmds].num_args = d->fcmds[i].num_args;
		for (a = 0; a < d->fcmds[i].num_args; a++)
			out->cmds[out->num_cmds].args[a] = d->fcmds[i].args[a];
		out->num_cmds++;
	}
	return out;
}

float SINMDL_Frame_Time (int modelindex, int animnum, int framenum)
{
	sinmdl_def_t	*d = def_for_index (modelindex);
	float			t;
	int				n;

	(void)framenum;
	if (!d || animnum < 0 || animnum >= d->num_anims)
		return 0.1f;
	n = d->anims[animnum].num_frames;
	t = d->anims[animnum].time;
	if (n < 1)
		n = 1;
	if (t <= 0.f)
		return 0.1f;
	return t / (float)n;
}

void SINMDL_Frame_Delta (int modelindex, int animnum, int framenum, vec3_t delta)
{
	(void)modelindex;
	(void)animnum;
	(void)framenum;
	VectorClear (delta);
}

const char *SINMDL_Anim_NameForNum (int modelindex, int animnum)
{
	sinmdl_def_t	*d = def_for_index (modelindex);
	if (!d || animnum < 0 || animnum >= d->num_anims)
		return "";
	return d->anims[animnum].name;
}

int SINMDL_Anim_NumForName (int modelindex, const char *name)
{
	sinmdl_def_t	*d = def_for_index (modelindex);
	int				i;

	if (!d || !name || !name[0])
		return -1;
	for (i = 0; i < d->num_anims; i++)
	{
		if (!sin_strcasecmp (d->anims[i].name, name))
			return i;
	}
	return -1;
}

/*
 * sin.exe (0x00488F80): Anim_Random matches all animations whose name starts
 * with `name` (case-insensitive) and where name[strlen(name)] != '_', then
 * picks one weighted by anim.weight (or returns -1 if none match).
 */
int SINMDL_Anim_Random (int modelindex, const char *name)
{
	sinmdl_def_t	*d = def_for_index (modelindex);
	size_t			len;
	int				matches[SINMDL_MAX_ANIMS];
	int				count = 0, i;
	float			total_weight = 0.f, r, acc = 0.f;

	if (!d || !name || !name[0])
		return -1;
	len = strlen (name);
	for (i = 0; i < d->num_anims; i++)
	{
		if (!sin_strncasecmp (d->anims[i].name, name, len)
			&& d->anims[i].name[len] != '_')
		{
			matches[count++] = i;
			total_weight += (d->anims[i].weight > 0.f) ? d->anims[i].weight : 1.0f;
		}
	}
	if (count == 0)
		return -1;
	if (count == 1 || total_weight <= 0.f)
		return matches[0];
	r = ((float)(rand () & 0x7fff) / 32767.0f) * total_weight;
	for (i = 0; i < count; i++)
	{
		float	w = (d->anims[matches[i]].weight > 0.f) ? d->anims[matches[i]].weight : 1.0f;
		acc += w;
		if (r <= acc)
			return matches[i];
	}
	return matches[count - 1];
}

int SINMDL_Anim_NumFrames (int modelindex, int animnum)
{
	sinmdl_def_t	*d = def_for_index (modelindex);
	if (!d || animnum < 0 || animnum >= d->num_anims)
		return 0;
	return d->anims[animnum].num_frames;
}

float SINMDL_Anim_Time (int modelindex, int animnum)
{
	sinmdl_def_t	*d = def_for_index (modelindex);
	if (!d || animnum < 0 || animnum >= d->num_anims)
		return 0.f;
	return d->anims[animnum].time;
}

void SINMDL_Anim_Delta (int modelindex, int animnum, vec3_t delta)
{
	(void)modelindex;
	(void)animnum;
	VectorClear (delta);
}

static sinmdl_bone_t *
bone_by_name (sinmdl_def_t *d, const char *bonename)
{
	int	i;

	if (!d || !bonename)
		return NULL;
	for (i = 0; i < d->num_bones; i++)
	{
		if (!sin_strcasecmp (d->bones[i].name, bonename))
			return &d->bones[i];
	}
	return NULL;
}

const char *SINMDL_GetBoneGroupName (int modelindex, const char *bonename)
{
	sinmdl_def_t	*d = def_for_index (modelindex);
	sinmdl_bone_t	*b = bone_by_name (d, bonename);

	return (b && b->group_name[0]) ? b->group_name : "";
}

int SINMDL_Group_NameToNum (int modelindex, const char *name)
{
	sinmdl_def_t	*d = def_for_index (modelindex);
	int				i;

	if (!d || !name)
		return -1;
	for (i = 0; i < d->num_groups; i++)
	{
		if (!sin_strcasecmp (d->groups[i], name))
			return i;
	}
	return -1;
}

const char *SINMDL_Group_NumToName (int modelindex, int num)
{
	sinmdl_def_t	*d = def_for_index (modelindex);

	if (!d || num < 0 || num >= d->num_groups)
		return "";
	return d->groups[num];
}

qboolean SINMDL_GetBoneInfo (int modelindex, const char *bonename,
	int *groupindex, int *tri_num, vec3_t orientation)
{
	sinmdl_bone_t	*b;
	static int		fail_log;

	if (orientation)
		VectorClear (orientation);
	if (bonename && !sin_strcasecmp (bonename, "origin"))
	{
		if (groupindex)
			*groupindex = 0;
		if (tri_num)
			*tri_num = 0xfff;
		return true;
	}
	b = bone_by_name (def_for_index (modelindex), bonename);
	if (!b || b->surf_index < 0)
	{
		if ((fail_log++ % 20) == 0 && bonename)
			Com_DPrintf ("SINMDL_GetBoneInfo fail mi=%d bone=%s\n",
				modelindex, bonename);
		return false;
	}
	if (groupindex)
		*groupindex = b->surf_index;
	if (tri_num)
		*tri_num = b->tri_num;
	if (orientation)
		VectorCopy (b->orientation, orientation);
	return true;
}

qboolean SINMDL_GetBoneTransform (int modelindex, int groupindex, int tri_num,
	vec3_t orientation, int anim, int frame, float scale,
	vec3_t trans[3], vec3_t pos)
{
	sinmdl_def_t	*d = def_for_index (modelindex);
	sinmdl_bone_t	*b = NULL;
	int				i, r, c;
	vec3_t			angles;
	vec3_t			mat_rot[3], mat_tri[3];
	float			sc;

	(void)anim;
	(void)frame;

	if (pos)
		VectorClear (pos);
	if (trans)
	{
		VectorSet (trans[0], 1.f, 0.f, 0.f);
		VectorSet (trans[1], 0.f, 1.f, 0.f);
		VectorSet (trans[2], 0.f, 0.f, 1.f);
	}
	if (!d)
		return false;

	if (tri_num == 0xfff)
	{
		if (orientation)
			VectorCopy (orientation, angles);
		else
			VectorClear (angles);
		if (trans)
		{
			AngleVectors (angles, trans[0], trans[1], trans[2]);
			VectorNegate (trans[1], trans[1]);
		}
		return true;
	}

	if (groupindex < 0)
		return false;

	for (i = 0; i < d->num_bones; i++)
	{
		if (d->bones[i].surf_index == groupindex)
		{
			b = &d->bones[i];
			break;
		}
	}
	if (!b || !b->have_pos)
		return false;

	sc = (scale > 0.f ? scale : 1.f) * (d->scale != 0.f ? d->scale : 1.f);
	if (pos)
	{
		pos[0] = b->local_pos[0] * sc;
		pos[1] = b->local_pos[1] * sc;
		pos[2] = b->local_pos[2] * sc;
	}
	if (orientation)
		VectorCopy (orientation, angles);
	else
		VectorCopy (b->orientation, angles);
	if (trans)
	{
		AngleVectors (angles, mat_rot[0], mat_rot[1], mat_rot[2]);
		VectorNegate (mat_rot[1], mat_rot[1]);
		if (b->have_tri_v)
		{
			vec3_t	d1, d2;

			/* sin.exe (0x0048A18C + 0x00456DE0 + 0x004567D0) */
			VectorSubtract (b->tri_v[2], b->tri_v[0], d1);
			VectorNormalize (d1);
			VectorSubtract (b->tri_v[1], b->tri_v[0], d2);
			VectorNormalize (d2);
			CrossProduct (d2, d1, mat_tri[0]);
			CrossProduct (d2, mat_tri[0], mat_tri[1]);
			VectorNegate (d2, mat_tri[2]);
			for (r = 0; r < 3; r++)
			{
				for (c = 0; c < 3; c++)
				{
					trans[r][c] = mat_rot[r][0] * mat_tri[0][c]
						+ mat_rot[r][1] * mat_tri[1][c]
						+ mat_rot[r][2] * mat_tri[2][c];
				}
			}
		}
		else
		{
			VectorCopy (mat_rot[0], trans[0]);
			VectorCopy (mat_rot[1], trans[1]);
			VectorCopy (mat_rot[2], trans[2]);
		}
	}
	return true;
}

#endif /* !REF_GL */

#endif /* SIN_MP */
