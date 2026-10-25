/*
 * SINMDL mesh load + draw for ref_gl.
 * SBM topology + multi-SAM anims; frame lerp; gunanim selects .sam.
 */
#include "gl_local.h"

#ifdef SIN_MP

extern int		modfilelen;

#define SBM_MAGIC		0x204d4253u	/* 'SBM ' */
#define SAM_MAGIC		0x204d4153u	/* 'SAM ' */
#define SDEF_MAGIC		0x46454453u	/* 'SDEF' */

typedef struct
{
	char	label[SINMDL_ANIM_NAME];
	char	file[MAX_QPATH];
} sin_def_anim_t;

typedef struct
{
	char	path[MAX_QPATH];
	char	sbm[MAX_QPATH];
	char	skin[MAX_QPATH];
	int		num_skins;
	char	skins[SINMDL_MAX_SKINS][MAX_QPATH];
	float	scale;
	vec3_t	origin;
	int		num_anims;
	sin_def_anim_t	anims[SINMDL_MAX_ANIMS];
} sin_def_parse_t;

static qboolean
sin_endswith (const char *s, const char *ext)
{
	size_t	n, e;

	if (!s || !ext)
		return false;
	n = strlen (s);
	e = strlen (ext);
	if (n < e)
		return false;
	return !Q_strcasecmp ((char *)s + n - e, (char *)ext);
}

static void
sin_join (char *out, int outsz, const char *dir, const char *file)
{
	if (dir && dir[0])
		Com_sprintf (out, outsz, "%s/%s", dir, file);
	else
		Com_sprintf (out, outsz, "%s", file);
}

static void
sin_parse_def (const char *text, int len, sin_def_parse_t *out)
{
	char		line[256];
	const char	*p, *end;

	memset (out, 0, sizeof (*out));
	out->scale = 1.f;
	if (len < 4 || *(const unsigned *)text != SDEF_MAGIC)
		return;
	p = text + 4;
	end = text + len;

	while (p < end)
	{
		const char	*nl;
		size_t		L;
		char		*c;
		char		*tok, *rest;

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

		c = strstr (line, "//");
		if (c)
			*c = 0;
		c = strstr (line, "/*");
		if (c)
			*c = 0;
		L = strlen (line);
		while (L > 0 && (line[L - 1] == ' ' || line[L - 1] == '\t'))
			line[--L] = 0;
		if (!line[0] || line[0] == '!' || line[0] == '*')
			continue;

		tok = line;
		while (*tok == ' ' || *tok == '\t')
			tok++;
		rest = tok;
		while (*rest && *rest != ' ' && *rest != '\t')
			rest++;
		if (*rest)
			*rest++ = 0;
		while (*rest == ' ' || *rest == '\t')
			rest++;

		if (!Q_strcasecmp (tok, "path") && rest[0])
		{
			Com_sprintf (out->path, sizeof (out->path), "%s", rest);
			continue;
		}
		if (!Q_strcasecmp (tok, "scale") && rest[0])
		{
			out->scale = (float)atof (rest);
			if (out->scale <= 0.f)
				out->scale = 1.f;
			continue;
		}
		if (!Q_strcasecmp (tok, "origin") && rest[0])
		{
			char	*r = rest;

			out->origin[0] = (float)atof (r);
			while (*r && *r != ' ' && *r != '\t') r++;
			while (*r == ' ' || *r == '\t') r++;
			if (*r)
			{
				out->origin[1] = (float)atof (r);
				while (*r && *r != ' ' && *r != '\t') r++;
				while (*r == ' ' || *r == '\t') r++;
				if (*r)
					out->origin[2] = (float)atof (r);
			}
			continue;
		}
		if (sin_endswith (tok, ".sbm") && !out->sbm[0])
		{
			Com_sprintf (out->sbm, sizeof (out->sbm), "%s", tok);
			continue;
		}
		if (sin_endswith (rest, ".sbm") && !out->sbm[0])
		{
			Com_sprintf (out->sbm, sizeof (out->sbm), "%s", rest);
			continue;
		}
		if (sin_endswith (rest, ".sam") && out->num_anims < SINMDL_MAX_ANIMS)
		{
			sin_def_anim_t	*a = &out->anims[out->num_anims++];

			if (!sin_endswith (tok, ".sam"))
				Com_sprintf (a->label, sizeof (a->label), "%s", tok);
			else
				a->label[0] = 0;
			Com_sprintf (a->file, sizeof (a->file), "%s", rest);
			continue;
		}
		if (sin_endswith (tok, ".sam") && out->num_anims < SINMDL_MAX_ANIMS)
		{
			sin_def_anim_t	*a = &out->anims[out->num_anims++];

			a->label[0] = 0;
			Com_sprintf (a->file, sizeof (a->file), "%s", tok);
			continue;
		}
		if (sin_endswith (rest, ".tga"))
		{
			if (out->num_skins < SINMDL_MAX_SKINS)
				Com_sprintf (out->skins[out->num_skins++], sizeof (out->skins[0]), "%s", rest);
			if (!out->skin[0])
				Com_sprintf (out->skin, sizeof (out->skin), "%s", rest);
			continue;
		}
		else if (sin_endswith (tok, ".tga"))
		{
			if (out->num_skins < SINMDL_MAX_SKINS)
				Com_sprintf (out->skins[out->num_skins++], sizeof (out->skins[0]), "%s", tok);
			if (!out->skin[0])
				Com_sprintf (out->skin, sizeof (out->skin), "%s", tok);
			continue;
		}
	}
}

static qboolean
sin_load_file (const char *path, byte **buf, int *len)
{
	*len = ri.FS_LoadFile ((char *)path, (void **)buf);
	return *len > 0 && *buf != NULL;
}

static void
sin_decode_into (sinmdl_t *m, sinmdl_anim_t *a, int nxyz, int frame, vec3_t *out)
{
	int			i;
	float		sx, sy, sz, tx, ty, tz;
	const byte	*verts;

	if (!m || !a || nxyz < 1 || a->nframes < 1 || !a->frame_verts)
		return;
	if (frame < 0)
		frame = 0;
	if (frame >= a->nframes)
		frame %= a->nframes;

	sx = a->frame_scale[frame * 3 + 0];
	sy = a->frame_scale[frame * 3 + 1];
	sz = a->frame_scale[frame * 3 + 2];
	tx = a->frame_translate[frame * 3 + 0] + m->def_origin[0];
	ty = a->frame_translate[frame * 3 + 1] + m->def_origin[1];
	tz = a->frame_translate[frame * 3 + 2] + m->def_origin[2];
	verts = a->frame_verts + frame * nxyz * 4;

	for (i = 0; i < nxyz; i++)
	{
		out[i][0] = (float)verts[i * 4 + 0] * sx + tx;
		out[i][1] = (float)verts[i * 4 + 1] * sy + ty;
		out[i][2] = (float)verts[i * 4 + 2] * sz + tz;
	}
}

static void
sin_setup_pose (sinmdl_t *m, int anim, int frame, int oldframe, float backlerp)
{
	sinmdl_anim_t	*a;
	int				i;
	float			frontlerp;

	if (!m || m->num_anims < 1 || m->nxyz < 1)
		return;
	if (anim < 0 || anim >= m->num_anims)
		anim = 0;
	a = &m->anims[anim];
	if (a->nframes < 1)
		return;

	if (frame < 0)
		frame = 0;
	if (frame >= a->nframes)
		frame %= a->nframes;
	if (oldframe < 0 || oldframe >= a->nframes)
		oldframe = frame;

	if (anim == m->last_anim && frame == m->last_frame
		&& oldframe == m->last_oldframe && backlerp < 0.001f)
		return;

	if (backlerp < 0.001f || oldframe == frame || !m->xyz_old)
	{
		sin_decode_into (m, a, m->nxyz, frame, m->xyz);
	}
	else
	{
		frontlerp = 1.f - backlerp;
		sin_decode_into (m, a, m->nxyz, oldframe, m->xyz_old);
		sin_decode_into (m, a, m->nxyz, frame, m->xyz);
		for (i = 0; i < m->nxyz; i++)
		{
			m->xyz[i][0] = m->xyz_old[i][0] * backlerp + m->xyz[i][0] * frontlerp;
			m->xyz[i][1] = m->xyz_old[i][1] * backlerp + m->xyz[i][1] * frontlerp;
			m->xyz[i][2] = m->xyz_old[i][2] * backlerp + m->xyz[i][2] * frontlerp;
		}
	}
	m->last_anim = anim;
	m->last_frame = frame;
	m->last_oldframe = oldframe;
}

static qboolean
sin_load_one_sam (const byte *sam, int len, int nxyz, sinmdl_anim_t *a,
	const char *label, const vec3_t def_origin, vec3_t mins, vec3_t maxs, qboolean update_bounds)
{
	const unsigned	*dw;
	int				nv, nframes, fofs, f, i;
	float			fps;

	if (len < 0x80)
		return false;
	dw = (const unsigned *)sam;
	if (dw[0] != SAM_MAGIC || dw[1] != 1)
		return false;
	nv = (int)dw[0x1c];
	nframes = (int)dw[0x1d];
	fofs = (int)dw[0x1e];
	fps = LittleFloat (*(float *)(sam + 0x1b * 4));
	if (nv != nxyz || nframes < 1 || nframes > 512 || fofs < 0
		|| fofs + nframes * 0x2c > len)
		return false;

	if (label && label[0])
		Com_sprintf (a->name, sizeof (a->name), "%s", label);
	else
	{
		const char	*nm = (const char *)sam + 8;

		Com_sprintf (a->name, sizeof (a->name), "%s", nm);
	}
	a->nframes = nframes;
	a->fps = (fps > 0.5f && fps < 60.f) ? fps : 10.f;
	a->frame_scale = Hunk_Alloc (nframes * 3 * sizeof (float));
	a->frame_translate = Hunk_Alloc (nframes * 3 * sizeof (float));
	a->frame_verts = Hunk_Alloc (nframes * nv * 4);

	for (f = 0; f < nframes; f++)
	{
		int			fh = fofs + f * 0x2c;
		int			rel;
		const byte	*src;
		float		sx, sy, sz, tx, ty, tz;

		sx = LittleFloat (*(float *)(sam + fh + 0x10));
		sy = LittleFloat (*(float *)(sam + fh + 0x14));
		sz = LittleFloat (*(float *)(sam + fh + 0x18));
		tx = LittleFloat (*(float *)(sam + fh + 0x1c));
		ty = LittleFloat (*(float *)(sam + fh + 0x20));
		tz = LittleFloat (*(float *)(sam + fh + 0x24));
		rel = LittleLong (*(int *)(sam + fh + 0x28));
		if (rel < 0 || fh + rel + nv * 4 > len)
			return false;

		a->frame_scale[f * 3 + 0] = sx;
		a->frame_scale[f * 3 + 1] = sy;
		a->frame_scale[f * 3 + 2] = sz;
		a->frame_translate[f * 3 + 0] = tx;
		a->frame_translate[f * 3 + 1] = ty;
		a->frame_translate[f * 3 + 2] = tz;
		src = sam + fh + rel;
		memcpy (a->frame_verts + f * nv * 4, src, nv * 4);

		if (!update_bounds)
			continue;
		for (i = 0; i < nv; i++)
		{
			float	x = (float)src[i * 4 + 0] * sx + tx + def_origin[0];
			float	y = (float)src[i * 4 + 1] * sy + ty + def_origin[1];
			float	z = (float)src[i * 4 + 2] * sz + tz + def_origin[2];

			if (x < mins[0]) mins[0] = x;
			if (y < mins[1]) mins[1] = y;
			if (z < mins[2]) mins[2] = z;
			if (x > maxs[0]) maxs[0] = x;
			if (y > maxs[1]) maxs[1] = y;
			if (z > maxs[2]) maxs[2] = z;
		}
	}
	return true;
}

static qboolean
sin_load_sbm_mesh (const byte *sbm, int len, sinmdl_t *m)
{
	const unsigned	*dw;
	int				nxyz, nst, nsurfs, ofs_st, s, t, i;
	float			*st;
	const byte		*base;

	if (len < 28)
		return false;
	dw = (const unsigned *)sbm;
	if (dw[0] != SBM_MAGIC || dw[1] != 1)
		return false;
	nxyz = (int)dw[2];
	nst = (int)dw[3];
	nsurfs = (int)dw[4];
	ofs_st = (int)dw[5];
	if (nxyz < 1 || nxyz > 2048 || nst < 1 || nsurfs < 1 || nsurfs > SINMDL_MAX_SURFS)
		return false;
	if (ofs_st < 0 || ofs_st + nst * 8 > len)
		return false;

	m->nxyz = nxyz;
	m->nst = nst;
	m->nsurfs = nsurfs;
	m->xyz = Hunk_Alloc (nxyz * sizeof (vec3_t));
	m->xyz_old = Hunk_Alloc (nxyz * sizeof (vec3_t));
	m->st = Hunk_Alloc (nst * 2 * sizeof (float));
	m->last_anim = -1;
	m->last_frame = -1;
	m->last_oldframe = -1;
	st = m->st;
	base = sbm + ofs_st;
	for (i = 0; i < nst; i++)
	{
		/* SiN STs often tile (>1); do not clamp to 0..1. */
		st[i * 2 + 0] = LittleFloat (*(float *)(base + i * 8));
		st[i * 2 + 1] = LittleFloat (*(float *)(base + i * 8 + 4));
	}

	for (s = 0; s < nsurfs; s++)
	{
		int				so = 28 + s * 24;
		int				ntris, tofs;
		unsigned short	*idx;
		const byte		*tris;

		if (so + 24 > len)
			return false;
		ntris = LittleLong (*(int *)(sbm + so + 4));
		tofs = LittleLong (*(int *)(sbm + so + 16));
		if (ntris < 0 || ntris > 8192 || tofs < 0 || so + tofs + ntris * 16 > len)
			return false;
		m->surfs[s].idx = Hunk_Alloc (ntris * 6 * sizeof (unsigned short));
		idx = m->surfs[s].idx;
		tris = sbm + so + tofs;
		m->surfs[s].ntris = 0;
		for (t = 0; t < ntris; t++)
		{
			const byte		*tr = tris + t * 16;
			unsigned short	xyz[3], sti[3];
			int				ok = 1;
			int				o;

			/*
			 * 16B tri: xyz[3] then st[3] (u16 each), then 4B unused.
			 * Interleaved (xyz,st)×3 rejects valid tris and soup-meshes the rest.
			 */
			for (i = 0; i < 3; i++)
			{
				xyz[i] = (unsigned short)LittleShort (*(short *)(tr + i * 2));
				sti[i] = (unsigned short)LittleShort (*(short *)(tr + 6 + i * 2));
				if (xyz[i] >= (unsigned)nxyz || sti[i] >= (unsigned)nst)
					ok = 0;
			}
			if (!ok)
				continue;
			o = m->surfs[s].ntris * 6;
			idx[o + 0] = xyz[0]; idx[o + 1] = sti[0];
			idx[o + 2] = xyz[1]; idx[o + 3] = sti[1];
			idx[o + 4] = xyz[2]; idx[o + 5] = sti[2];
			m->surfs[s].ntris++;
		}
	}
	return true;
}

static int
sin_pick_default_anim (sinmdl_t *m)
{
	int	i;

	for (i = 0; i < m->num_anims; i++)
		if (!Q_strcasecmp (m->anims[i].name, "primaryidle")
			|| !Q_strcasecmp (m->anims[i].name, "secondaryidle"))
			return i;
	for (i = 0; i < m->num_anims; i++)
		if (!Q_strcasecmp (m->anims[i].name, "ready")
			|| !Q_strcasecmp (m->anims[i].name, "secondaryready"))
			return i;
	return 0;
}

void Mod_LoadSinDef (model_t *mod, void *buffer)
{
	sinmdl_t		*m;
	sin_def_parse_t	parsed;
	char			sbm_path[MAX_QPATH], skin_path[MAX_QPATH], sam_path[MAX_QPATH];
	byte			*sbm_buf = NULL, *sam_buf = NULL;
	int				sbm_len = 0, sam_len = 0, i;
	vec3_t			mins, maxs;

	mod->type = mod_sinmdl;
	mod->numframes = 1;
	VectorSet (mod->mins, -4, -8, -8);
	VectorSet (mod->maxs, 20, 8, 8);
	mod->radius = 24;

	if (!buffer || modfilelen < 4 || *(unsigned *)buffer != SDEF_MAGIC)
	{
		Hunk_Alloc (1);
		return;
	}

	m = Hunk_Alloc (sizeof (*m));
	memset (m, 0, sizeof (*m));

	sin_parse_def ((const char *)buffer, modfilelen, &parsed);
	if (!parsed.sbm[0] || parsed.num_anims < 1)
	{
		ri.Con_Printf (PRINT_DEVELOPER, "Mod_LoadSinDef: %s missing sbm/sam\n", mod->name);
		return;
	}

	sin_join (sbm_path, sizeof (sbm_path), parsed.path, parsed.sbm);

	if (!sin_load_file (sbm_path, &sbm_buf, &sbm_len))
	{
		ri.Con_Printf (PRINT_ALL, "Mod_LoadSinDef: can't load %s\n", sbm_path);
		return;
	}
	if (!sin_load_sbm_mesh (sbm_buf, sbm_len, m))
	{
		ri.Con_Printf (PRINT_DEVELOPER, "Mod_LoadSinDef: bad SBM %s\n", sbm_path);
		ri.FS_FreeFile (sbm_buf);
		return;
	}
	ri.FS_FreeFile (sbm_buf);

	m->def_scale = parsed.scale;
	VectorCopy (parsed.origin, m->def_origin);

	mins[0] = mins[1] = mins[2] = 1e9f;
	maxs[0] = maxs[1] = maxs[2] = -1e9f;

	for (i = 0; i < parsed.num_anims && m->num_anims < SINMDL_MAX_ANIMS; i++)
	{
		sin_join (sam_path, sizeof (sam_path), parsed.path, parsed.anims[i].file);
		if (!sin_load_file (sam_path, &sam_buf, &sam_len))
		{
			ri.Con_Printf (PRINT_DEVELOPER, "Mod_LoadSinDef: skip %s\n", sam_path);
			continue;
		}
		if (!sin_load_one_sam (sam_buf, sam_len, m->nxyz, &m->anims[m->num_anims],
				parsed.anims[i].label, m->def_origin, mins, maxs, true))
		{
			ri.Con_Printf (PRINT_DEVELOPER, "Mod_LoadSinDef: bad SAM %s\n", sam_path);
			ri.FS_FreeFile (sam_buf);
			continue;
		}
		ri.FS_FreeFile (sam_buf);
		m->num_anims++;
	}

	if (m->num_anims < 1)
	{
		ri.Con_Printf (PRINT_ALL, "Mod_LoadSinDef: no usable SAMs for %s\n", mod->name);
		return;
	}

	m->center[0] = (mins[0] + maxs[0]) * 0.5f;
	m->center[1] = (mins[1] + maxs[1]) * 0.5f;
	m->center[2] = (mins[2] + maxs[2]) * 0.5f;
	VectorCopy (mins, mod->mins);
	VectorCopy (maxs, mod->maxs);
	mod->numframes = m->anims[0].nframes;
	m->radius = (float)sqrt (
		(maxs[0] - mins[0]) * (maxs[0] - mins[0]) +
		(maxs[1] - mins[1]) * (maxs[1] - mins[1]) +
		(maxs[2] - mins[2]) * (maxs[2] - mins[2])) * 0.5f;
	if (m->radius < 1.f)
		m->radius = 1.f;
	/* Bounds include .def scale so culling matches draw. */
	{
		float	sc = m->def_scale;

		for (i = 0; i < 3; i++)
		{
			mod->mins[i] *= sc;
			mod->maxs[i] *= sc;
		}
		mod->radius = m->radius * sc;
	}
	if (mod->radius < 8.f)
		mod->radius = 8.f;

	m->num_skins = 0;
	for (i = 0; i < parsed.num_skins && i < SINMDL_MAX_SKINS; i++)
	{
		sin_join (skin_path, sizeof (skin_path), parsed.path, parsed.skins[i]);
		m->skins[m->num_skins] = GL_FindImage (skin_path, it_skin);
		if (m->skins[m->num_skins])
			m->num_skins++;
	}
	if (m->num_skins > 0)
		m->skin = m->skins[0];
	else if (parsed.skin[0])
	{
		sin_join (skin_path, sizeof (skin_path), parsed.path, parsed.skin);
		m->skin = GL_FindImage (skin_path, it_skin);
	}

	sin_setup_pose (m, sin_pick_default_anim (m), 0, 0, 0.f);

	ri.Con_Printf (PRINT_ALL, "Mod_LoadSinDef: %s -> %s (%d verts, %d anims, %d skins, scale=%.2f)%s\n",
		mod->name, sbm_path, m->nxyz, m->num_anims, m->num_skins, m->def_scale,
		m->skin ? "" : " [no skin]");
}

void R_DrawSinModel (entity_t *e)
{
	sinmdl_t	*m;
	vec3_t		shadelight;
	int			s, t, c, anim, fr, ofr;
	float		backlerp;
	static int	vg_draw_log;
	image_t		*drawskin;

	if (!currentmodel)
		return;

	m = NULL;
	if (currentmodel->extradatasize >= (int)sizeof (sinmdl_t))
		m = (sinmdl_t *)currentmodel->extradata;

	anim = 0;
	fr = 0;
	ofr = 0;
	backlerp = e->backlerp;
	if (m && m->num_anims > 0 && m->nxyz > 0)
	{
		qboolean	world = !(e->flags & RF_WEAPONMODEL);

		anim = e->sin_anim;
		if (anim < 0 || anim >= m->num_anims)
			anim = sin_pick_default_anim (m);

		/*
		 * World/TP: trust networked anim+frame (frame 0 is valid).
		 * Viewgun: time-loop idle when frame unset.
		 */
		if (e->frame >= 0 && e->frame < m->anims[anim].nframes)
		{
			fr = e->frame;
			if (e->oldframe >= 0 && e->oldframe < m->anims[anim].nframes)
				ofr = e->oldframe;
			else
				ofr = fr;
		}
		else if (m->anims[anim].nframes > 1)
		{
			float	ft = r_newrefdef.time * m->anims[anim].fps;

			fr = (int)ft % m->anims[anim].nframes;
			ofr = (fr + m->anims[anim].nframes - 1) % m->anims[anim].nframes;
			backlerp = 1.f - (ft - (float)(int)ft);
		}

		sin_setup_pose (m, anim, fr, ofr, backlerp);
	}

	if ((e->flags & RF_WEAPONMODEL) && (vg_draw_log++ % 300) == 0)
		ri.Con_Printf (PRINT_DEVELOPER, "R_DrawSinModel view %s a=%d f=%d/%d\n",
			currentmodel->name,
			anim, fr, (m && anim < m->num_anims) ? m->anims[anim].nframes : 0);

	if (e->flags & RF_FULLBRIGHT || e->flags & RF_WEAPONMODEL)
		shadelight[0] = shadelight[1] = shadelight[2] = 1.0f;
	else
		R_LightPoint (e->origin, shadelight);

	if (e->flags & (RF_DEPTHHACK | RF_WEAPONMODEL))
		qglDepthRange (gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin));

	qglPushMatrix ();
	e->angles[PITCH] = -e->angles[PITCH];
	R_RotateForEntity (e);
	e->angles[PITCH] = -e->angles[PITCH];

	{
		float	sc = (m && m->def_scale > 0.f) ? m->def_scale : 1.f;
		if (sc != 1.f)
			qglScalef (sc, sc, sc);
	}

	drawskin = NULL;
	if (e->skin)
		drawskin = e->skin;
	else if (m && e->skinnum >= 0 && e->skinnum < m->num_skins && m->skins[e->skinnum])
		drawskin = m->skins[e->skinnum];
	else if (m && m->skin)
		drawskin = m->skin;

	if (!m || m->nxyz < 1 || m->nsurfs < 1 || !m->xyz)
	{
		vec3_t	mins, maxs;

		VectorCopy (currentmodel->mins, mins);
		VectorCopy (currentmodel->maxs, maxs);
		qglDisable (GL_TEXTURE_2D);
		qglColor3fv (shadelight);
		qglBegin (GL_QUADS);
		qglVertex3f (mins[0], mins[1], maxs[2]);
		qglVertex3f (maxs[0], mins[1], maxs[2]);
		qglVertex3f (maxs[0], maxs[1], maxs[2]);
		qglVertex3f (mins[0], maxs[1], maxs[2]);
		qglVertex3f (mins[0], mins[1], mins[2]);
		qglVertex3f (mins[0], maxs[1], mins[2]);
		qglVertex3f (maxs[0], maxs[1], mins[2]);
		qglVertex3f (maxs[0], mins[1], mins[2]);
		qglVertex3f (mins[0], maxs[1], mins[2]);
		qglVertex3f (mins[0], maxs[1], maxs[2]);
		qglVertex3f (maxs[0], maxs[1], maxs[2]);
		qglVertex3f (maxs[0], maxs[1], mins[2]);
		qglVertex3f (mins[0], mins[1], mins[2]);
		qglVertex3f (maxs[0], mins[1], mins[2]);
		qglVertex3f (maxs[0], mins[1], maxs[2]);
		qglVertex3f (mins[0], mins[1], maxs[2]);
		qglVertex3f (maxs[0], mins[1], mins[2]);
		qglVertex3f (maxs[0], maxs[1], mins[2]);
		qglVertex3f (maxs[0], maxs[1], maxs[2]);
		qglVertex3f (maxs[0], mins[1], maxs[2]);
		qglVertex3f (mins[0], mins[1], mins[2]);
		qglVertex3f (mins[0], mins[1], maxs[2]);
		qglVertex3f (mins[0], maxs[1], maxs[2]);
		qglVertex3f (mins[0], maxs[1], mins[2]);
		qglEnd ();
		qglEnable (GL_TEXTURE_2D);
	}
	else
	{
		qglColor3fv (shadelight);
		if (drawskin)
		{
			GL_Bind (drawskin->texnum);
			qglEnable (GL_TEXTURE_2D);
		}
		else
			qglDisable (GL_TEXTURE_2D);

		qglBegin (GL_TRIANGLES);
		for (s = 0; s < m->nsurfs; s++)
		{
			unsigned short	*idx = m->surfs[s].idx;
			int				ntris = m->surfs[s].ntris;

			if (!idx || ntris < 1)
				continue;
			for (t = 0; t < ntris; t++)
			{
				for (c = 0; c < 3; c++)
				{
					unsigned short	vi = idx[t * 6 + c * 2 + 0];
					unsigned short	si = idx[t * 6 + c * 2 + 1];

					if (drawskin && m->st)
						qglTexCoord2f (m->st[si * 2], m->st[si * 2 + 1]);
					qglVertex3fv (m->xyz[vi]);
				}
			}
		}
		qglEnd ();
		if (!drawskin)
			qglEnable (GL_TEXTURE_2D);
	}

	qglColor3f (1, 1, 1);
	qglPopMatrix ();



	if (e->flags & (RF_DEPTHHACK | RF_WEAPONMODEL))
		qglDepthRange (gldepthmin, gldepthmax);
}

#endif /* SIN_MP */
