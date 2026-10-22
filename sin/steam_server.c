/*
 * Steam GameServer auth stubs for SiN MP.
 * Real Steamworks wiring goes here when SDK is available; stub accepts
 * local/dev connects so protocol-7 bring-up works without Steam.
 *
 * REA anchors: CSteamServer_BeginAuthSession 0x4992e0,
 * CSteamServer_GetAuthSessionTicket_Local 0x4993b0,
 * CSteamServer_OnAuthTicketResponse 0x499260.
 */
#include "steam_server.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef SIN_MP

/* Matches server.h MAX_CHALLENGES */
#ifndef SIN_MAX_CHALLENGES
#define SIN_MAX_CHALLENGES	1024
#endif

cvar_t *sv_steamauth;

typedef struct {
	sin_steamid_t	steamid;
	byte			ticket[SIN_MAX_AUTH_TICKET];
	int				ticket_len;
	qboolean		used;
} sin_ticket_slot_t;

static sin_ticket_slot_t	slots[SIN_MAX_CHALLENGES];
static qboolean				steam_ready;

void SinSteam_InitServer (void)
{
	sv_steamauth = Cvar_Get ("sv_steamauth", "0", 0);
	memset (slots, 0, sizeof(slots));
	steam_ready = false;
	/*
	 * TODO: SteamGameServer_Init + callbacks when Steamworks SDK is linked
	 * (SIN_STEAMWORKS=ON). Until then stub mode logs and allows connect.
	 */
	Com_Printf ("SinSteam: stub server auth (sv_steamauth=%s)\n", sv_steamauth->string);
}

void SinSteam_ShutdownServer (void)
{
	memset (slots, 0, sizeof(slots));
	steam_ready = false;
}

qboolean SinSteam_ParseChallengeArgs (sin_steamid_t *out_steamid, byte *ticket, int ticket_cap,
	int *out_ticket_len)
{
	char *hex;
	int tlen = 0;

	if (!out_steamid || !ticket || !out_ticket_len || ticket_cap <= 0)
		return false;

	*out_steamid = strtoull (Cmd_Argv(1), NULL, 10);
	hex = Cmd_Argv(2);
	while (hex[0] && hex[1] && tlen < ticket_cap)
	{
		unsigned int b = 0;
		if (sscanf (hex, "%2x", &b) != 1)
			break;
		ticket[tlen++] = (byte)b;
		hex += 2;
	}
	*out_ticket_len = tlen;
	return true;
}

qboolean SinSteam_StoreChallengeTicket (int challenge_index, sin_steamid_t steamid,
	const byte *ticket, int ticket_len)
{
	sin_ticket_slot_t *s;

	if (challenge_index < 0 || challenge_index >= SIN_MAX_CHALLENGES)
		return false;
	if (ticket_len < 0 || ticket_len > SIN_MAX_AUTH_TICKET)
		return false;

	s = &slots[challenge_index];
	memset (s, 0, sizeof(*s));
	s->steamid = steamid;
	s->ticket_len = ticket_len;
	if (ticket && ticket_len > 0)
		memcpy (s->ticket, ticket, ticket_len);
	s->used = true;
	Com_DPrintf ("SinSteam: stored ticket for challenge slot %i steamid %llu len %i\n",
		challenge_index, (unsigned long long)steamid, ticket_len);
	return true;
}

qboolean SinSteam_BeginAuthSession (sin_steamid_t steamid, const byte *ticket, int ticket_len)
{
	if (sv_steamauth && sv_steamauth->value && !steam_ready)
	{
		Com_Printf ("SinSteam: sv_steamauth set but Steamworks not linked — rejecting\n");
		return false;
	}
	(void)steamid;
	(void)ticket;
	(void)ticket_len;
	Com_DPrintf ("SinSteam: BeginAuthSession stub OK\n");
	return true;
}

qboolean SinSteam_BeginAuthSessionLocal (void)
{
	Com_DPrintf ("SinSteam: local auth ticket stub OK\n");
	return true;
}

#endif /* SIN_MP */
