/*
 * SiN MP entity_state delta (Steam sin.exe FUN_0047b790 / FUN_00401af0).
 * Include once per TU with SIN_ENTITY_DELTA_WRITE and/or SIN_ENTITY_DELTA_PARSE.
 */
#if defined(SIN_MP) && defined(SIN_ENTITY_DELTA_WRITE)

/* Clamp float→byte; garbage/NaN from game DLL must not Com_Error WriteByte */
static int
sin_frac8 (float f)
{
	int	v;

	if (f != f)	/* NaN */
		return 0;
	v = (int)(f * 255.0f);
	if (v < 0)
		return 0;
	if (v > 255)
		return 255;
	return v;
}

static int
sin_scale16 (float f)
{
	int	v;

	if (f != f)
		return 0;
	v = (int)(f * 256.0f);
	if (v < -32768)
		return -32768;
	if (v > 32767)
		return 32767;
	return v;
}

static void
Sin_MSG_WriteDeltaEntity (entity_state_t *from, entity_state_t *to,
	sizebuf_t *msg, qboolean force, qboolean newentity)
{
	unsigned	bits;
	unsigned	eff;
	int			i;

	if (!to->number)
		Com_Error (ERR_FATAL, "Unset entity number");
	if (to->number >= MAX_EDICTS)
		Com_Error (ERR_FATAL, "Entity number >= MAX_EDICTS");

	bits = 0;
	if (to->number >= 256)
		bits |= U_NUMBER16;

	if (to->origin[0] != from->origin[0])
		bits |= U_ORIGIN1;
	if (to->origin[1] != from->origin[1])
		bits |= U_ORIGIN2;
	if (to->origin[2] != from->origin[2])
		bits |= U_ORIGIN3;

	if (to->angles[0] != from->angles[0])
		bits |= U_ANGLE1;
	if (to->angles[1] != from->angles[1])
		bits |= U_ANGLE2;
	if (to->angles[2] != from->angles[2])
		bits |= U_ANGLE3;

	if (to->skinnum != from->skinnum)
	{
		if ((unsigned)to->skinnum < 256)
			bits |= U_SKIN8;
		else if ((unsigned)to->skinnum < 0x10000)
			bits |= U_SKIN16;
		else
			bits |= (U_SKIN8 | U_SKIN16);
	}

	if (to->anim != from->anim)
		bits |= U_SIN_ANIM;
	else if (to->frame != from->frame)
		bits |= U_FRAME8;

	if ((short)sin_scale16 (to->scale) != (short)sin_scale16 (from->scale))
		bits |= U_SIN_SCALE;

	if (sin_frac8 (to->alpha) != sin_frac8 (from->alpha))
		bits |= U_SIN_ALPHA;

	if (sin_frac8 (to->color_r) != sin_frac8 (from->color_r)
		|| sin_frac8 (to->color_g) != sin_frac8 (from->color_g)
		|| sin_frac8 (to->color_b) != sin_frac8 (from->color_b)
		|| to->radius != from->radius)
		bits |= U_SIN_COLOR;

	if (to->parent != from->parent
		|| to->bone.group_num != from->bone.group_num
		|| to->bone.tri_num != from->bone.tri_num
		|| to->bone.orientation[0] != from->bone.orientation[0]
		|| to->bone.orientation[1] != from->bone.orientation[1]
		|| to->bone.orientation[2] != from->bone.orientation[2])
		bits |= U_SIN_BONE;

	{
		int	ng = to->numgroups;
		int	fg = from->numgroups;

		if (ng < 0)
			ng = 0;
		if (ng > MAX_MODEL_GROUPS)
			ng = MAX_MODEL_GROUPS;
		if (fg < 0)
			fg = 0;
		if (fg > MAX_MODEL_GROUPS)
			fg = MAX_MODEL_GROUPS;
		if (ng != fg || (ng > 0 && memcmp (to->groups, from->groups, ng)))
			bits |= U_SIN_GROUPS;
	}

	if ((to->gunanim & 255) != (from->gunanim & 255)
		|| (to->gunframe & 255) != (from->gunframe & 255))
		bits |= U_SIN_GUN;

	if (to->effects != from->effects)
		bits |= U_SIN_EFFECTS;

	if (to->renderfx != from->renderfx)
		bits |= U_SIN_RENDERFX;

	if (to->solid != from->solid)
		bits |= U_SOLID;

	if ((char)to->modelindex != (char)from->modelindex)
		bits |= U_MODEL;
	if (((to->modelindex ^ from->modelindex) & 0xffffff00) != 0)
		bits |= U_SIN_MODEL_HI;

	if ((char)to->gunmodelindex != (char)from->gunmodelindex)
		bits |= U_SIN_GUNMODEL_LO;
	if (((to->gunmodelindex ^ from->gunmodelindex) & 0xffffff00) != 0)
		bits |= U_SIN_GUNMODEL_HI;

	if (to->sound != from->sound)
		bits |= U_SOUND;

	if (newentity || (to->renderfx & RF_BEAM)
		|| (to->renderfx & 0x80) || (to->effects & 0x800)
		|| to->old_origin[0] != from->old_origin[0]
		|| to->old_origin[1] != from->old_origin[1]
		|| to->old_origin[2] != from->old_origin[2])
		bits |= U_OLDORIGIN;

	if (to->lightofs != from->lightofs)
		bits |= U_SIN_LIGHTOFS;

	/*
	 * Unused prevframe is -1. Never emit the bit for negative values — the wire
	 * is a byte, so -1 became 255 and stock ref_gl crashed in MOD_LoadBmodel.
	 */
	if (to->prevframe >= 0 && to->prevframe != from->prevframe)
		bits |= U_SIN_PREVFRAME;

	if (!bits && !force)
		return;

	if (bits & 0xff000000)
		bits |= U_MOREBITS3 | U_MOREBITS2 | U_MOREBITS1;
	else if (bits & 0x00ff0000)
		bits |= U_MOREBITS2 | U_MOREBITS1;
	else if (bits & 0x0000ff00)
		bits |= U_MOREBITS1;

	MSG_WriteByte (msg, bits & 255);
	if (bits & 0xff000000)
	{
		MSG_WriteByte (msg, (bits >> 8) & 255);
		MSG_WriteByte (msg, (bits >> 16) & 255);
		MSG_WriteByte (msg, (bits >> 24) & 255);
	}
	else if (bits & 0x00ff0000)
	{
		MSG_WriteByte (msg, (bits >> 8) & 255);
		MSG_WriteByte (msg, (bits >> 16) & 255);
	}
	else if (bits & 0x0000ff00)
		MSG_WriteByte (msg, (bits >> 8) & 255);

	if (bits & U_NUMBER16)
		MSG_WriteShort (msg, to->number);
	else
		MSG_WriteByte (msg, to->number);

	if (bits & U_MODEL)
		MSG_WriteByte (msg, to->modelindex & 255);
	if (bits & U_SIN_GUNMODEL_LO)
		MSG_WriteByte (msg, to->gunmodelindex & 255);
	if (bits & U_SIN_MODEL_HI)
		MSG_WriteByte (msg, (to->modelindex >> 8) & 255);
	if (bits & U_SIN_GUNMODEL_HI)
		MSG_WriteByte (msg, (to->gunmodelindex >> 8) & 255);

	if (bits & U_FRAME8)
		MSG_WriteByte (msg, to->frame);
	if (bits & U_SIN_ANIM)
		MSG_WriteShort (msg, (to->frame & 255) | ((to->anim & 255) << 8));

	if (bits & U_SIN_SCALE)
		MSG_WriteShort (msg, sin_scale16 (to->scale));

	if (bits & U_SIN_ALPHA)
		MSG_WriteByte (msg, sin_frac8 (to->alpha));

	if (bits & U_SIN_COLOR)
	{
		MSG_WriteByte (msg, sin_frac8 (to->color_r));
		MSG_WriteByte (msg, sin_frac8 (to->color_g));
		MSG_WriteByte (msg, sin_frac8 (to->color_b));
		MSG_WriteShort (msg, to->radius);
	}

	if (bits & U_SIN_BONE)
	{
		MSG_WriteShort (msg, to->parent);
		MSG_WriteShort (msg, ((to->bone.group_num & 15) << 12)
			| (to->bone.tri_num & 0xfff));
		MSG_WriteByte (msg, (int)(to->bone.orientation[0] * (256.0f / 360.0f)) & 255);
		MSG_WriteByte (msg, (int)(to->bone.orientation[1] * (256.0f / 360.0f)) & 255);
		MSG_WriteByte (msg, (int)(to->bone.orientation[2] * (256.0f / 360.0f)) & 255);
	}

	if (bits & U_SIN_GROUPS)
	{
		int	ng = to->numgroups;

		if (ng < 0)
			ng = 0;
		if (ng > MAX_MODEL_GROUPS)
			ng = MAX_MODEL_GROUPS;
		MSG_WriteByte (msg, ng);
		for (i = 0; i < ng; i++)
			MSG_WriteByte (msg, to->groups[i]);
	}

	if (bits & U_SIN_GUN)
	{
		MSG_WriteByte (msg, to->gunanim & 255);
		MSG_WriteByte (msg, to->gunframe & 255);
	}

	if ((bits & (U_SKIN8 | U_SKIN16)) == (U_SKIN8 | U_SKIN16))
		MSG_WriteLong (msg, to->skinnum);
	else if (bits & U_SKIN8)
		MSG_WriteByte (msg, to->skinnum);
	else if (bits & U_SKIN16)
		MSG_WriteShort (msg, to->skinnum);

	if (bits & U_SIN_EFFECTS)
	{
		/* Steam wire MOREBITS in effects bytes 7/15 — do not mutate entity. */
		eff = to->effects & ~0x00008080u;
		if (eff & 0xffff0000u)
			eff |= 0x8080u;
		else if (eff & 0xff00u)
			eff |= 0x80u;
		MSG_WriteByte (msg, eff & 255);
		if ((signed char)(eff & 255) < 0)
			MSG_WriteByte (msg, (eff >> 8) & 255);
		if (eff & 0x8000u)
			MSG_WriteShort (msg, (eff >> 16) & 0xffff);
	}

	if (bits & U_SIN_RENDERFX)
		MSG_WriteShort (msg, to->renderfx);

	if (bits & U_ORIGIN1)
		MSG_WriteCoord (msg, to->origin[0]);
	if (bits & U_ORIGIN2)
		MSG_WriteCoord (msg, to->origin[1]);
	if (bits & U_ORIGIN3)
		MSG_WriteCoord (msg, to->origin[2]);

	if (to->effects & 0x10)
	{
		if (bits & U_ANGLE1)
			MSG_WriteAngle16 (msg, to->angles[0]);
		if (bits & U_ANGLE2)
			MSG_WriteAngle16 (msg, to->angles[1]);
		if (bits & U_ANGLE3)
			MSG_WriteAngle16 (msg, to->angles[2]);
	}
	else
	{
		if (bits & U_ANGLE1)
			MSG_WriteAngle (msg, to->angles[0]);
		if (bits & U_ANGLE2)
			MSG_WriteAngle (msg, to->angles[1]);
		if (bits & U_ANGLE3)
			MSG_WriteAngle (msg, to->angles[2]);
	}

	if (bits & U_OLDORIGIN)
	{
		MSG_WriteCoord (msg, to->old_origin[0]);
		MSG_WriteCoord (msg, to->old_origin[1]);
		MSG_WriteCoord (msg, to->old_origin[2]);
	}

	if (bits & U_SOUND)
		MSG_WriteShort (msg, to->sound);
	if (bits & U_SOLID)
		MSG_WriteLong (msg, to->solid);
	if (bits & U_SIN_LIGHTOFS)
	{
		i = to->lightofs;
		if (i < -128)
			i = -128;
		if (i > 127)
			i = 127;
		MSG_WriteChar (msg, i);
	}
	if (bits & U_SIN_PREVFRAME)
		MSG_WriteByte (msg, to->prevframe & 255);
}

#endif /* SIN_ENTITY_DELTA_WRITE */

#if defined(SIN_MP) && defined(SIN_ENTITY_DELTA_PARSE)

static void
Sin_CL_ParseDelta (entity_state_t *from, entity_state_t *to, int number, int bits)
{
	int		i;
	int		b;
	unsigned	eff;

	*to = *from;
	to->number = number;
	to->prevframe = -1;

	if (bits & U_MODEL)
		to->modelindex = (to->modelindex & ~0xff) | (MSG_ReadByte (&net_message) & 0xff);
	if (bits & U_SIN_GUNMODEL_LO)
		to->gunmodelindex = (to->gunmodelindex & ~0xff) | (MSG_ReadByte (&net_message) & 0xff);
	if (bits & U_SIN_MODEL_HI)
		to->modelindex = (to->modelindex & 0xff) | (MSG_ReadByte (&net_message) << 8);
	if (bits & U_SIN_GUNMODEL_HI)
		to->gunmodelindex = (to->gunmodelindex & 0xff) | (MSG_ReadByte (&net_message) << 8);

	if (bits & U_FRAME8)
		to->frame = MSG_ReadByte (&net_message);
	if (bits & U_SIN_ANIM)
	{
		b = MSG_ReadShort (&net_message);
		to->frame = b & 0xff;
		to->anim = (b >> 8) & 0xff;
	}

	if (bits & U_SIN_SCALE)
		to->scale = MSG_ReadShort (&net_message) * (1.0f / 256.0f);

	if (bits & U_SIN_ALPHA)
		to->alpha = MSG_ReadByte (&net_message) / 255.0f;

	if (bits & U_SIN_COLOR)
	{
		to->color_r = MSG_ReadByte (&net_message) / 255.0f;
		to->color_g = MSG_ReadByte (&net_message) / 255.0f;
		to->color_b = MSG_ReadByte (&net_message) / 255.0f;
		to->radius = MSG_ReadShort (&net_message);
	}

	if (bits & U_SIN_BONE)
	{
		to->parent = MSG_ReadShort (&net_message);
		b = MSG_ReadShort (&net_message);
		to->bone.group_num = (b >> 12) & 15;
		to->bone.tri_num = b & 0xfff;
		to->bone.orientation[0] = MSG_ReadChar (&net_message) * (360.0f / 256.0f);
		to->bone.orientation[1] = MSG_ReadChar (&net_message) * (360.0f / 256.0f);
		to->bone.orientation[2] = MSG_ReadChar (&net_message) * (360.0f / 256.0f);
	}

	if (bits & U_SIN_GROUPS)
	{
		int	ng = MSG_ReadByte (&net_message);

		if (ng < 0)
			ng = 0;
		to->numgroups = (ng > MAX_MODEL_GROUPS) ? MAX_MODEL_GROUPS : ng;
		for (i = 0; i < ng; i++)
		{
			b = MSG_ReadByte (&net_message);
			if (i < MAX_MODEL_GROUPS)
				to->groups[i] = (byte)b;
		}
	}

	if (bits & U_SIN_GUN)
	{
		to->gunanim = MSG_ReadByte (&net_message);
		to->gunframe = MSG_ReadByte (&net_message);
	}

	if ((bits & (U_SKIN8 | U_SKIN16)) == (U_SKIN8 | U_SKIN16))
		to->skinnum = MSG_ReadLong (&net_message);
	else if (bits & U_SKIN8)
		to->skinnum = MSG_ReadByte (&net_message);
	else if (bits & U_SKIN16)
		to->skinnum = MSG_ReadShort (&net_message);

	if (bits & U_SIN_EFFECTS)
	{
		eff = MSG_ReadByte (&net_message);
		if ((signed char)eff < 0)
			eff |= MSG_ReadByte (&net_message) << 8;
		if (eff & 0x8000u)
			eff |= ((unsigned)MSG_ReadShort (&net_message)) << 16;
		to->effects = eff;
	}

	if (bits & U_SIN_RENDERFX)
		to->renderfx = MSG_ReadShort (&net_message);

	if (bits & U_ORIGIN1)
		to->origin[0] = MSG_ReadCoord (&net_message);
	if (bits & U_ORIGIN2)
		to->origin[1] = MSG_ReadCoord (&net_message);
	if (bits & U_ORIGIN3)
		to->origin[2] = MSG_ReadCoord (&net_message);

	if (to->effects & 0x10)
	{
		if (bits & U_ANGLE1)
			to->angles[0] = MSG_ReadAngle16 (&net_message);
		if (bits & U_ANGLE2)
			to->angles[1] = MSG_ReadAngle16 (&net_message);
		if (bits & U_ANGLE3)
			to->angles[2] = MSG_ReadAngle16 (&net_message);
	}
	else
	{
		if (bits & U_ANGLE1)
			to->angles[0] = MSG_ReadAngle (&net_message);
		if (bits & U_ANGLE2)
			to->angles[1] = MSG_ReadAngle (&net_message);
		if (bits & U_ANGLE3)
			to->angles[2] = MSG_ReadAngle (&net_message);
	}

	if (bits & U_OLDORIGIN)
		MSG_ReadPos (&net_message, to->old_origin);

	if (bits & U_SOUND)
		to->sound = MSG_ReadShort (&net_message);
	if (bits & U_SOLID)
		to->solid = MSG_ReadLong (&net_message);
	if (bits & U_SIN_LIGHTOFS)
		to->lightofs = MSG_ReadChar (&net_message);
	if (bits & U_SIN_PREVFRAME)
		to->prevframe = MSG_ReadByte (&net_message);
}

#endif /* SIN_ENTITY_DELTA_PARSE */
