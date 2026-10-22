/*
 * SiN Multiplayer protocol constants (from Steam sin.exe REA).
 * Stock Quake 2 used PROTOCOL_VERSION 34; Steam SiN MP uses 7.
 */
#ifndef SIN_PROTOCOL_H
#define SIN_PROTOCOL_H

#ifdef SIN_MP
#undef PROTOCOL_VERSION
#define PROTOCOL_VERSION	7

/*
 * Stock client *destination* default is 27015 (REA htons(0x6987)), but the
 * Steam client also *binds* 27015 locally. Dedicated stays on Q2 27910;
 * connect with host:27910 to avoid the bind clash.
 */

/*
 * Entity delta bit remaps vs Q2 (FUN_0047b790 / FUN_00401af0).
 * Shared names keep MOREBITS / NUMBER / ORIGIN / ANGLE / SKIN / OLDORIGIN.
 */
#define U_SIN_GROUPS		(1<<13)		/* was unused in Q2; numgroups + bytes */
#define U_SIN_SCALE			U_EFFECTS8	/* short scale*256 */
#define U_SIN_PREVFRAME		U_RENDERFX8	/* byte prevframe (default -1) */
#define U_SIN_GUN			U_EVENT		/* gunanim + gunframe bytes */
#define U_SIN_ANIM			U_FRAME16	/* short: frame | (anim<<8) */
#define U_SIN_GUNMODEL_LO	U_MODEL2	/* gunmodelindex low byte */
#define U_SIN_MODEL_HI		U_MODEL3	/* modelindex high byte */
#define U_SIN_GUNMODEL_HI	U_MODEL4	/* gunmodelindex high byte */
#define U_SIN_EFFECTS		U_EFFECTS16	/* variable-length effects */
#define U_SIN_RENDERFX		U_RENDERFX16	/* renderfx as short */
#define U_SIN_BONE			(1<<28)		/* parent + bone + orientation */
#define U_SIN_ALPHA			(1<<29)		/* alpha*255 */
#define U_SIN_COLOR			(1<<30)		/* rgb bytes + radius short */
#define U_SIN_LIGHTOFS		(1u<<31)	/* signed char lightofs */

/*
 * Playerstate flags — Steam CL_ParsePlayerstate 0x404810 (not Q2 bit layout).
 * Gravity axis pack (bit 5) omitted until pmove gravity-dir is wired.
 */
#define	SIN_PS_M_TYPE		(1<<0)
#define	SIN_PS_M_ORIGIN		(1<<1)
#define	SIN_PS_M_VELOCITY	(1<<2)
#define	SIN_PS_M_FLAGS		(1<<3)		/* ushort: 1 byte, or 2 if bit7 set */
#define	SIN_PS_M_DELTA_ANGLES	(1<<4)
#define	SIN_PS_M_GRAVITY	(1<<5)		/* packed gravity+axis — skip for now */
#define	SIN_PS_M_TIME		(1<<6)
#define	SIN_PS_GUN		(1<<7)		/* 6 chars: gunangles then gunoffset */
#define	SIN_PS_BLEND		(1<<8)
#define	SIN_PS_FOV		(1<<9)
#define	SIN_PS_RDFLAGS		(1<<10)
#define	SIN_PS_VIEWOFFSET	(1<<11)		/* char, char, short (Z) */
#define	SIN_PS_VIEWANGLES	(1<<12)
#define	SIN_PS_MUSIC		(1<<13)		/* mood | (fallback<<4) */
#define	SIN_PS_KICKANGLES	(1<<14)

#define SDEF_MAGIC			0x46454453u	/* 'SDEF' */
#define SPRT_MAGIC			0x54525053u	/* 'SPRT' — SiN sprite script */
#endif

/* Max Steam auth ticket blob stored on a challenge slot (REA / Steamworks). */
#ifndef SIN_MAX_AUTH_TICKET
#define SIN_MAX_AUTH_TICKET	1024
#endif

#endif
